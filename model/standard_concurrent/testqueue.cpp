#include "Concurrent_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <iomanip>
#include <numeric>
#include <cassert>

// 计时工具类
class Timer {
private:
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point start_;

public:
    Timer() : start_(Clock::now()) {}

    double elapsed_ms() const {
        auto end = Clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

    double elapsed_sec() const {
        return elapsed_ms() / 1000.0;
    }
};

// 测试1：基本功能测试（单线程）
template <typename Queue>
void test_basic_functions() {
    std::cout << "测试1：基本功能测试...";
    
    Queue q;
    assert(q.empty() == true);
    assert(q.size() == 0);

    // 测试push和pop
    q.push(10);
    assert(q.empty() == false);
    assert(q.size() == 1);

    int val;
    assert(q.try_pop(val) == true);
    assert(val == 10);
    assert(q.empty() == true);

    // 测试emplace
    q.emplace(20);
    assert(q.try_pop(val) == true);
    assert(val == 20);

    // 测试移动语义
    q.push(std::move(30));
    assert(q.try_pop(val) == true);
    assert(val == 30);

    // 测试快照
    q.push(1);
    q.push(2);
    q.push(3);
    auto snapshot = q.snapshot();
    assert(snapshot.size() == 3);
    assert(snapshot[0] == 1 && snapshot[1] == 2 && snapshot[2] == 3);

    // 测试clear
    q.clear();
    assert(q.empty() == true);

    std::cout << "通过\n";
}

// 测试2：多生产者单消费者
template <typename Queue>
void test_multi_producer_single_consumer(size_t data_size, size_t producer_count) {
    std::cout << "测试2：多生产者(" << producer_count << ")单消费者...";
    
    Queue q;
    std::atomic<size_t> produced(0);
    std::atomic<size_t> consumed(0);
    std::atomic<size_t> checksum(0);

    // 生产者线程
    std::vector<std::thread> producers;
    for (size_t i = 0; i < producer_count; ++i) {
        producers.emplace_back([&, id = i]() {
            for (size_t j = 0; j < data_size; ++j) {
                size_t value = id * data_size + j;
                q.push(value);
                checksum.fetch_add(value, std::memory_order_relaxed);
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // 消费者线程
    std::thread consumer([&]() {
        size_t local_checksum = 0;
        int value;
        while (consumed.load(std::memory_order_relaxed) < data_size * producer_count) {
            if (q.pop(value)) {  // 使用阻塞式pop
                local_checksum += value;
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
        }
        // 验证校验和
        assert(local_checksum == checksum.load(std::memory_order_relaxed));
    });

    // 等待所有线程完成
    for (auto& t : producers) t.join();
    consumer.join();

    // 验证所有数据都被处理
    assert(produced == consumed);
    assert(q.empty() == true);

    std::cout << "通过 (总数据量: " << produced << ")\n";
}

// 测试3：单生产者多消费者
template <typename Queue>
void test_single_producer_multi_consumer(size_t data_size, size_t consumer_count) {
    std::cout << "测试3：单生产者多消费者(" << consumer_count << ")...";
    
    Queue q;
    std::atomic<size_t> produced(0);
    std::atomic<size_t> consumed(0);
    std::atomic<size_t> checksum(0);

    // 生产者线程
    std::thread producer([&]() {
        for (size_t i = 0; i < data_size; ++i) {
            q.push(i);
            checksum.fetch_add(i, std::memory_order_relaxed);
            produced.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // 消费者线程
    std::vector<std::thread> consumers;
    std::vector<size_t> local_checksums(consumer_count, 0);
    
    for (size_t i = 0; i < consumer_count; ++i) {
        consumers.emplace_back([&, id = i]() {
            int value;
            while (consumed.load(std::memory_order_relaxed) < data_size) {
                if (q.try_pop(value)) {  // 使用非阻塞式try_pop
                    local_checksums[id] += value;
                    consumed.fetch_add(1, std::memory_order_relaxed);
                } else {
                    // 短暂休眠避免CPU空转
                    std::this_thread::yield();
                }
            }
        });
    }

    // 等待所有线程完成
    producer.join();
    for (auto& t : consumers) t.join();

    // 验证校验和
    size_t total_checksum = std::accumulate(local_checksums.begin(), 
                                           local_checksums.end(), 0ULL);
    assert(total_checksum == checksum.load(std::memory_order_relaxed));
    
    // 验证所有数据都被处理
    assert(produced == consumed);
    assert(q.empty() == true);

    std::cout << "通过 (总数据量: " << produced << ")\n";
}

// 测试4：多生产者多消费者性能测试
template <typename Queue>
void test_performance(size_t data_size, size_t producer_count, size_t consumer_count) {
    std::cout << "测试4：性能测试 (" 
              << producer_count << "生产者, " 
              << consumer_count << "消费者)...";
    
    Queue q;
    std::atomic<bool> start_flag(false);
    std::atomic<size_t> produced(0);
    std::atomic<size_t> consumed(0);

    // 生产者线程
    std::vector<std::thread> producers;
    for (size_t i = 0; i < producer_count; ++i) {
        producers.emplace_back([&]() {
            // 等待开始信号
            while (!start_flag.load(std::memory_order_relaxed));
            
            for (size_t j = 0; j < data_size; ++j) {
                q.push(1);  // 推送简单数据
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    // 消费者线程
    std::vector<std::thread> consumers;
    for (size_t i = 0; i < consumer_count; ++i) {
        consumers.emplace_back([&]() {
            // 等待开始信号
            while (!start_flag.load(std::memory_order_relaxed));
            
            int value;
            while (consumed.load(std::memory_order_relaxed) < data_size * producer_count) {
                if (q.pop(value)) {  // 使用阻塞式pop
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    // 开始计时并启动测试
    Timer timer;
    start_flag.store(true, std::memory_order_relaxed);

    // 等待所有线程完成
    for (auto& t : producers) t.join();
    for (auto& t : consumers) t.join();

    // 计算性能
    double elapsed = timer.elapsed_sec();
    double total_ops = produced + consumed;  // 入队+出队总操作数
    double ops_per_sec = total_ops / elapsed;

    // 验证所有数据都被处理
    assert(produced == consumed);
    assert(q.empty() == true);

    std::cout << "完成\n";
    std::cout << "  总数据量: " << produced << "\n";
    std::cout << "  耗时: " << std::fixed << std::setprecision(2) << elapsed << "秒\n";
    std::cout << "  吞吐量: " << std::fixed << std::setprecision(2) 
              << ops_per_sec / 1000 << " Kops/秒\n";
}

// 测试5：超时测试
template <typename Queue>
void test_timeout() {
    std::cout << "测试5：超时测试...";
    
    Queue q;
    int value;
    // auto start = std::chrono::steady_clock::now();
    
    // 队列为空时尝试出队，应该超时
    bool result = false;
    std::thread t([&]() 
    {
        // 注意：原代码没有带超时的pop，这里使用try_pop循环模拟
        auto timeout = std::chrono::milliseconds(100);
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout) {
            if (q.try_pop(value)) {
                result = true;
                return;
            }
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
        result = false;
    });
    
    t.join();
    assert(result == false);  // 应该超时
    
    // 测试超时后入队能否被正确处理
    q.push(100);
    assert(q.try_pop(value) == true);
    assert(value == 100);

    std::cout << "通过\n";
}

int main() {
    using QueueType = multi_concurrent::concurrent_queue<int>;
    const size_t BASE_DATA_SIZE = 10;
    const size_t PRODUCER_COUNT = 32;
    const size_t CONSUMER_COUNT = 32;

    std::cout << "===== concurrent_queue 多线程测试 =====" << std::endl;
    std::cout << "CPU核心数: " << std::thread::hardware_concurrency() << std::endl << std::endl;

    // 运行各项测试
    test_basic_functions<QueueType>();
    test_multi_producer_single_consumer<QueueType>(BASE_DATA_SIZE, PRODUCER_COUNT);
    test_single_producer_multi_consumer<QueueType>(BASE_DATA_SIZE, CONSUMER_COUNT);
    test_timeout<QueueType>();
    test_performance<QueueType>(BASE_DATA_SIZE * 10, PRODUCER_COUNT, CONSUMER_COUNT);

    std::cout << std::endl << "所有测试通过!" << std::endl;
    return 0;
}
