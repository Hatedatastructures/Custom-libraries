// #include <iostream>
// #include <fstream>
// #include <string>
// #include <chrono>

// int main()
// {
//   std::ofstream file("test.txt");
//   auto time_start = std::chrono::high_resolution_clock::now();

//   for (int i = 0; i < 10000000; i++)
//   {
//     file << std::string("hello,word!   ") + std::to_string(i);
//     file.put('\n');
//   }
//   auto time_end = std::chrono::high_resolution_clock::now();
//   std::cerr << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(time_end - time_start).count() << "ms" << std::endl;

//   file.close();
//   return 0;
// }
#include <atomic>
#include <thread>
#include <vector>
#include <iostream>
#include <fstream>
#include <chrono>
#include <memory>
#include <cstring>
#include <emmintrin.h>
#include <boost/align/aligned_allocator.hpp>

// 日志条目结构（二进制序列化，减少格式化开销）
struct LogEntry {
    uint64_t timestamp;  // 微秒级时间戳
    uint32_t thread_id;  // 线程ID
    uint16_t level;      // 日志级别
    char data[256];      // 日志内容（预分配固定大小）
};

// 无锁环形缓冲区（单生产者单消费者）
template <size_t Capacity>
class SpscRingBuffer {
public:
    static_assert((Capacity & (Capacity - 1)) == 0, "容量必须为2的幂次");
    using AlignedAllocator = boost::alignment::aligned_allocator<LogEntry, 64>;
    using Buffer = std::vector<LogEntry, AlignedAllocator>;

    SpscRingBuffer() : buffer(Capacity) {
        // static_assert(sizeof(Buffer) % 64 == 0, "缓冲区需64字节对齐");
    }

    // 生产者写入（返回是否成功）
    bool push(const LogEntry& entry) {
        const auto head = head_.load(std::memory_order_relaxed);
        const auto next_head = (head + 1) & mask_;
        if (next_head == tail_.load(std::memory_order_acquire)) {
            return false;  // 缓冲区满
        }
        buffer[head] = entry;
        head_.store(next_head, std::memory_order_release);
        return true;
    }

    // 消费者读取（返回读取数量）
    size_t pop(std::vector<LogEntry>& out) {
        const auto tail = tail_.load(std::memory_order_relaxed);
        const auto head = head_.load(std::memory_order_acquire);
        if (tail == head) return 0;

        const size_t count = (head - tail) & mask_;
        out.reserve(out.size() + count);
        for (size_t i = 0; i < count; ++i) {
            out.push_back(buffer[(tail + i) & mask_]);
        }
        tail_.store(head, std::memory_order_release);
        return count;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire);
    }

private:
    static constexpr size_t mask_ = Capacity - 1;
    Buffer buffer;
    alignas(64) std::atomic<size_t> head_{0};  // 缓存行对齐，避免假共享
    alignas(64) std::atomic<size_t> tail_{0};
};

// 线程局部日志队列（多生产者无锁写入）
class ThreadLocalLogger {
public:
    using RingBuffer = SpscRingBuffer<16384>;  // 16384 = 2^14

    ThreadLocalLogger() : buffer_(std::make_unique<RingBuffer>()) {}

    // 写入日志（线程安全，无锁）
    void write(const LogEntry& entry) {
        while (!buffer_->push(entry)) {
            _mm_pause();  // 缓冲区满时短暂等待
        }
    }

    // 消费者获取缓冲区数据
    size_t drain(std::vector<LogEntry>& out) {
        return buffer_->pop(out);
    }

private:
    std::unique_ptr<RingBuffer> buffer_;
};

// 全局日志管理器
class AsyncLogger {
public:
    AsyncLogger(const char* filename, size_t thread_count = 4) 
        : thread_count_(thread_count), 
          log_files_(thread_count),
          running_(true) {
        // 初始化线程局部缓冲区
        thread_buffers_.resize(thread_count);
        for (auto& buf : thread_buffers_) {
            buf = std::make_unique<ThreadLocalLogger>();
        }

        // 启动后台写线程（每个线程负责一个文件分片）
        for (size_t i = 0; i < thread_count; ++i) {
            log_files_[i].open(filename + std::to_string(i), 
                              std::ios::binary | std::ios::app);
            writers_.emplace_back(&AsyncLogger::write_loop, this, i);
        }
    }

    ~AsyncLogger() {
        running_ = false;
        for (auto& t : writers_) {
            t.join();
        }
        for (auto& f : log_files_) {
            if (f.is_open()) f.close();
        }
    }

    // 线程安全日志写入（根据线程ID分发到对应缓冲区）
    void log(const LogEntry& entry) {
        auto tid = std::hash<std::thread::id>{}(std::this_thread::get_id()) % thread_count_;
        thread_buffers_[tid]->write(entry);
    }

private:
    // 后台写线程循环（批量处理日志）
    void write_loop(size_t thread_idx) {
        std::vector<LogEntry> batch;
        batch.reserve(8192);  // 预分配批量处理缓冲区

        while (running_) {
            // 从对应线程缓冲区批量读取
            auto count = thread_buffers_[thread_idx]->drain(batch);
            if (count == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                continue;
            }

            // 批量写入文件（二进制模式减少转换开销）
            auto& file = log_files_[thread_idx];
            file.write(reinterpret_cast<const char*>(batch.data()), 
                      count * sizeof(LogEntry));

            // 每16MB刷新一次（平衡性能与可靠性）
            if (++flush_counter_ >= 16 * 1024 * 1024 / sizeof(LogEntry)) {
                file.flush();
                flush_counter_ = 0;
            }
            batch.clear();
        }
    }

    size_t thread_count_;
    std::vector<std::unique_ptr<ThreadLocalLogger>> thread_buffers_;
    std::vector<std::ofstream> log_files_;
    std::vector<std::thread> writers_;
    std::atomic<bool> running_;
    size_t flush_counter_ = 0;
};

// 性能测试示例
int main() {
    AsyncLogger logger("high_perf_log_", 16);  // 4个写线程，4个文件分片

    auto start = std::chrono::high_resolution_clock::now();
    const size_t total = 10'000'00000;  // 1000万条日志
    LogEntry entry{};
    strcpy(entry.data, "sample log message");

    // 多线程写入测试
    std::vector<std::thread> producers;
    for (int i = 0; i < 32; ++i) {  // 8个生产者线程
        producers.emplace_back([&, i]() {
            for (size_t j = 0; j < total / 16; ++j) {
                entry.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::high_resolution_clock::now().time_since_epoch()
                ).count();
                entry.thread_id = i;
                logger.log(entry);
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - start).count();
    std::cout << "写入完成，" << total / duration << "条/秒" << std::endl;

    return 0;
}