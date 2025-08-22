// /**
//  * @file test_atomic_containers.cpp
//  * @brief 无锁容器功能验证测试
//  * @author wang
//  * @version 1.0
//  * @date 2025-08-15
//  *
//  * 该文件用于验证所有无锁容器的基本功能是否正常工作
//  */

// #include "Atomic_container.hpp"
// #include <iostream>
// #include <thread>
// #include <vector>
// #include <chrono>
// #include <cassert>

// using namespace atomic_concurrent;

// // 测试 atomic_vector
// void test_atomic_vector()
// {
//     std::cout << "Testing atomic_vector..." << std::endl;

//     atomic_vector<int> vec;

//     // 基本操作测试
//     vec.push_back(1);
//     vec.push_back(2);
//     vec.push_back(3);

//     assert(vec.size() == 3);
//     assert(!vec.empty());

//     int value;
//     assert(vec.at(0, value) && value == 1);
//     assert(vec.at(1, value) && value == 2);
//     assert(vec.at(2, value) && value == 3);

//     assert(vec.pop_back(value) && value == 3);
//     assert(vec.size() == 2);

//     std::cout << "atomic_vector test passed!" << std::endl;
// }

// // 测试 atomic_queue
// void test_atomic_queue()
// {
//     std::cout << "Testing atomic_queue..." << std::endl;

//     atomic_queue<int> queue;

//     // 基本操作测试
//     queue.push(10);
//     queue.push(20);
//     queue.push(30);

//     assert(queue.size() == 3);
//     assert(!queue.empty());

//     int value;
//     assert(queue.try_pop(value) && value == 10);
//     assert(queue.try_pop(value) && value == 20);
//     assert(queue.try_pop(value) && value == 30);
//     assert(queue.empty());

//     std::cout << "atomic_queue test passed!" << std::endl;
// }

// // 测试 atomic_stack
// void test_atomic_stack()
// {
//     std::cout << "Testing atomic_stack..." << std::endl;

//     atomic_stack<int> stack;

//     // 基本操作测试
//     stack.push(100);
//     stack.push(200);
//     stack.push(300);

//     assert(stack.size() == 3);
//     assert(!stack.empty());

//     int value;
//     assert(stack.try_pop(value) && value == 300);
//     assert(stack.try_pop(value) && value == 200);
//     assert(stack.try_pop(value) && value == 100);
//     assert(stack.empty());

//     std::cout << "atomic_stack test passed!" << std::endl;
// }

// // 测试 atomic_list
// void test_atomic_list()
// {
//     std::cout << "Testing atomic_list..." << std::endl;

//     atomic_list<int> list;

//     // 基本操作测试
//     list.push_front(1);
//     list.push_back(2);
//     list.push_front(0);

//     assert(list.size() == 3);
//     assert(!list.empty());
//     assert(list.contains(0));
//     assert(list.contains(1));
//     assert(list.contains(2));

//     int value;
//     assert(list.pop_front(value) && value == 0);
//     assert(list.pop_back(value) && value == 2);
//     assert(list.size() == 1);

//     std::cout << "atomic_list test passed!" << std::endl;
// }

// // 测试 atomic_map
// void test_atomic_map()
// {
//     std::cout << "Testing atomic_map..." << std::endl;

//     atomic_map<int, std::string> map;

//     // 基本操作测试
//     assert(map.insert(1, "one"));
//     assert(map.insert(2, "two"));
//     assert(map.insert(3, "three"));
//     assert(!map.insert(1, "duplicate")); // 重复插入应该失败

//     assert(map.size() == 3);
//     assert(!map.empty());
//     assert(map.contains(1));
//     assert(map.contains(2));
//     assert(map.contains(3));
//     assert(!map.contains(4));

//     std::string value;
//     assert(map.find(2, value) && value == "two");

//     assert(map.erase(2));
//     assert(!map.contains(2));
//     assert(map.size() == 2);

//     std::cout << "atomic_map test passed!" << std::endl;
// }

// // 测试 atomic_set
// void test_atomic_set()
// {
//     std::cout << "Testing atomic_set..." << std::endl;

//     atomic_set<int> set;

//     // 基本操作测试
//     assert(set.insert(10));
//     assert(set.insert(20));
//     assert(set.insert(30));
//     assert(!set.insert(10)); // 重复插入应该失败

//     assert(set.size() == 3);
//     assert(!set.empty());
//     assert(set.contains(10));
//     assert(set.contains(20));
//     assert(set.contains(30));
//     assert(!set.contains(40));

//     assert(set.erase(20));
//     assert(!set.contains(20));
//     assert(set.size() == 2);

//     std::cout << "atomic_set test passed!" << std::endl;
// }

// // 测试 atomic_array
// void test_atomic_array()
// {
//     std::cout << "Testing atomic_array..." << std::endl;

//     atomic_array<int, 5> arr;

//     // 基本操作测试
//     assert(arr.size() == 5);
//     assert(!arr.empty());

//     arr.set(0, 100);
//     arr.set(1, 200);
//     arr.set(2, 300);

//     assert(arr[0] == 100);
//     assert(arr[1] == 200);
//     assert(arr[2] == 300);

//     arr.fill(999);
//     for (size_t i = 0; i < arr.size(); ++i)
//     {
//         assert(arr[i] == 999);
//     }

//     std::cout << "atomic_array test passed!" << std::endl;
// }

// // 测试 atomic_annular_queue
// void test_atomic_annular_queue()
// {
//     std::cout << "Testing atomic_annular_queue..." << std::endl;

//     atomic_annular_queue<int> queue(4); // 容量为4

//     // 基本操作测试
//     assert(queue.capacity() == 4);
//     assert(queue.empty());

//     assert(queue.try_push_back(1));
//     assert(queue.try_push_back(2));
//     assert(queue.try_push_back(3));
//     assert(queue.try_push_back(4));

//     assert(queue.full());
//     assert(!queue.try_push_back(5)); // 队列满，应该失败

//     int value;
//     assert(queue.try_pop_front(value) && value == 1);
//     assert(queue.try_pop_front(value) && value == 2);

//     assert(!queue.empty());
//     assert(!queue.full());

//     // 再次添加元素
//     assert(queue.try_push_back(5));
//     assert(queue.try_push_back(6));

//     std::cout << "atomic_annular_queue test passed!" << std::endl;
// }

// // 并发测试
// void concurrent_test()
// {
//     std::cout << "Running concurrent tests..." << std::endl;

//     atomic_queue<int> queue;
//     const int num_threads = 4;
//     const int items_per_thread = 1000;

//     // 生产者线程
//     std::vector<std::thread> producers;
//     for (int i = 0; i < num_threads; ++i)
//     {
//         producers.emplace_back([&queue, i, items_per_thread]()
//         {
//             for (int j = 0; j < items_per_thread; ++j)
//             {
//                 queue.push(i * items_per_thread + j);
//             }
//         });
//     }

//     // 消费者线程
//     std::vector<std::thread> consumers;
//     std::atomic<int> consumed_count(0);

//     for (int i = 0; i < num_threads; ++i)
//     {
//         consumers.emplace_back([&queue, &consumed_count]()
//         {
//             int value;
//             while (consumed_count.load() < num_threads * items_per_thread)
//             {
//                 if (queue.try_pop(value))
//                 {
//                     consumed_count.fetch_add(1);
//                 }
//                 else
//                 {
//                     std::this_thread::yield();
//                 }
//             }
//         });
//     }

//     // 等待所有线程完成
//     for (auto& t : producers)
//     {
//         t.join();
//     }

//     for (auto& t : consumers)
//     {
//         t.join();
//     }

//     assert(consumed_count.load() == num_threads * items_per_thread);
//     assert(queue.empty());

//     std::cout << "Concurrent test passed!" << std::endl;
// }

// int main()
// {
//     std::cout << "Starting atomic containers tests..." << std::endl;

//     try
//     {
//         test_atomic_vector();
//         test_atomic_queue();
//         test_atomic_stack();
//         test_atomic_list();
//         test_atomic_map();
//         test_atomic_set();
//         test_atomic_array();
//         test_atomic_annular_queue();

//         concurrent_test();

//         std::cout << "\nAll tests passed successfully!" << std::endl;
//         std::cout << "\n无锁容器库实现完成，所有容器的基本功能验证通过！" << std::endl;
//         std::cout << "\n已实现的容器包括：" << std::endl;
//         std::cout << "  - atomic_vector: 无锁动态数组" << std::endl;
//         std::cout << "  - atomic_queue: 无锁FIFO队列" << std::endl;
//         std::cout << "  - atomic_stack: 无锁LIFO栈" << std::endl;
//         std::cout << "  - atomic_list: 无锁双向链表" << std::endl;
//         std::cout << "  - atomic_map: 无锁关联容器（基于跳表）" << std::endl;
//         std::cout << "  - atomic_set: 无锁集合容器（基于跳表）" << std::endl;
//         std::cout << "  - atomic_array: 无锁固定大小数组" << std::endl;
//         std::cout << "  - atomic_annular_queue: 无锁环形队列" << std::endl;
//     }
//     catch (const std::exception& e)
//     {
//         std::cerr << "Test failed with exception: " << e.what() << std::endl;
//         return 1;
//     }

//     return 0;
// }

#include "Atomic_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <numeric>
#include <iomanip>

// 测试配置
constexpr size_t TEST_DATA_SIZE = 1000000; // 总数据量（每个线程）
constexpr int TEST_ROUNDS = 3;             // 测试轮数（取平均值）
constexpr int MIN_THREADS = 1;             // 最小线程数
constexpr int MAX_THREADS = 8;             // 最大线程数（根据CPU核心数调整）

// 计时工具
class Timer
{
private:
    using Clock = std::chrono::high_resolution_clock;
    Clock::time_point start_;

public:
    Timer() : start_(Clock::now()) {}

    double elapsed_ms() const
    {
        auto end = Clock::now();
        return std::chrono::duration<double, std::milli>(end - start_).count();
    }

    double elapsed_sec() const
    {
        return elapsed_ms() / 1000.0;
    }
};

// 生产者线程函数（入队操作）
template <typename Queue>
void producer(Queue &q, size_t count, std::atomic<size_t> &completed)
{
    for (size_t i = 0; i < count; ++i)
    {
        q.push(i); // 入队简单整数（避免数据构造开销）
    }
    completed.fetch_add(1, std::memory_order_relaxed);
}

// 消费者线程函数（出队操作）
template <typename Queue>
void consumer(Queue &q, size_t count, std::atomic<size_t> &completed, std::atomic<size_t> &checksum)
{
    size_t local_sum = 0;
    size_t actual = 0;
    int value;

    while (actual < count)
    {
        if (q.try_pop(value))
        { // 非阻塞出队
            local_sum += value;
            actual++;
        }
    }

    checksum.fetch_add(local_sum, std::memory_order_relaxed);
    completed.fetch_add(1, std::memory_order_relaxed);
}

// 混合模式线程函数（同时进行入队和出队）
template <typename Queue>
void mixed_worker(Queue &q, size_t produce_count, size_t consume_count,
                  std::atomic<size_t> &completed, std::atomic<size_t> &checksum)
{
    // 先生产一部分数据
    for (size_t i = 0; i < produce_count; ++i)
    {
        q.push(i);
    }

    // 再消费一部分数据
    size_t local_sum = 0;
    size_t actual = 0;
    int value;

    while (actual < consume_count)
    {
        if (q.try_pop(value))
        {
            local_sum += value;
            actual++;
        }
    }

    checksum.fetch_add(local_sum, std::memory_order_relaxed);
    completed.fetch_add(1, std::memory_order_relaxed);
}

// 验证数据完整性（通过校验和）
bool verify_checksum(size_t total_count, size_t checksum)
{
    // 1+2+...+n = n*(n-1)/2（生产者入队的数据为0~count-1）
    size_t expected = (total_count - 1) * total_count / 2;
    return checksum == expected;
}

// 测试1：单生产者 + 单消费者
template <typename Queue>
double test_single_prod_single_cons()
{
    Queue q;
    Timer timer;
    std::atomic<size_t> prod_completed(0);
    std::atomic<size_t> cons_completed(0);
    std::atomic<size_t> checksum(0);

    // 启动生产者和消费者
    std::thread prod(producer<Queue>, std::ref(q), TEST_DATA_SIZE, std::ref(prod_completed));
    std::thread cons(consumer<Queue>, std::ref(q), TEST_DATA_SIZE, std::ref(cons_completed), std::ref(checksum));

    // 等待完成
    while (prod_completed.load() < 1 || cons_completed.load() < 1)
    {
        std::this_thread::yield();
    }

    double elapsed = timer.elapsed_sec();
    bool valid = verify_checksum(TEST_DATA_SIZE, checksum.load());

    prod.join();
    cons.join();

    if (!valid)
    {
        std::cerr << " [数据校验失败!]";
    }

    return (TEST_DATA_SIZE * 2) / elapsed; // 总操作数（入队+出队）/ 时间
}

// 测试2：多生产者 + 多消费者（对称线程数）
template <typename Queue>
double test_multi_prod_multi_cons(size_t thread_count)
{
    if (thread_count % 2 != 0)
        thread_count++; // 确保偶数（一半生产者，一半消费者）
    int prod_count = thread_count / 2;
    int cons_count = thread_count / 2;
    size_t per_prod = TEST_DATA_SIZE / prod_count;
    size_t per_cons = TEST_DATA_SIZE / cons_count;

    Queue q;
    Timer timer;
    std::atomic<size_t> completed(0);
    std::atomic<size_t> checksum(0);
    std::vector<std::thread> threads;

    // 启动生产者
    for (int i = 0; i < prod_count; ++i)
    {
        threads.emplace_back(producer<Queue>, std::ref(q), per_prod, std::ref(completed));
    }

    // 启动消费者
    for (int i = 0; i < cons_count; ++i)
    {
        threads.emplace_back(consumer<Queue>, std::ref(q), per_cons, std::ref(completed), std::ref(checksum));
    }

    // 等待所有线程完成
    while (completed.load() < thread_count)
    {
        std::this_thread::yield();
    }

    double elapsed = timer.elapsed_sec();
    bool valid = verify_checksum(TEST_DATA_SIZE, checksum.load());

    for (auto &t : threads)
    {
        t.join();
    }

    if (!valid)
    {
        std::cerr << " [数据校验失败!]";
    }

    return (TEST_DATA_SIZE * 2) / elapsed; // 总操作数 / 时间
}

// 测试3：混合模式（每个线程既生产也消费）
template <typename Queue>
double test_mixed_mode(size_t thread_count)
{
    size_t per_thread_prod = TEST_DATA_SIZE / thread_count;
    size_t per_thread_cons = TEST_DATA_SIZE / thread_count;

    Queue q;
    Timer timer;
    std::atomic<size_t> completed(0);
    std::atomic<size_t> checksum(0);
    std::vector<std::thread> threads;

    // 启动混合线程
    for (size_t i = 0; i < thread_count; ++i)
    {
        threads.emplace_back(mixed_worker<Queue>, std::ref(q), per_thread_prod,
                             per_thread_cons, std::ref(completed), std::ref(checksum));
    }

    // 等待所有线程完成
    while (completed.load() < thread_count)
    {
        std::this_thread::yield();
    }

    double elapsed = timer.elapsed_sec();
    bool valid = verify_checksum(TEST_DATA_SIZE, checksum.load());

    for (auto &t : threads)
    {
        t.join();
    }

    if (!valid)
    {
        std::cerr << " [数据校验失败!]";
    }

    return (TEST_DATA_SIZE * 2) / elapsed; // 总操作数 / 时间
}

int main()
{
    using QueueType = atomic_concurrent::atomic_queue<int>;
    std::cout << "===== atomic_queue 性能测试 =====" << std::endl;
    std::cout << "测试数据量: " << TEST_DATA_SIZE << " 元素/线程" << std::endl;
    std::cout << "测试轮数: " << TEST_ROUNDS << " (取平均值)" << std::endl;
    std::cout << "CPU核心数参考: " << std::thread::hardware_concurrency() << std::endl
              << std::endl;

    // 测试1：单生产者单消费者
    std::cout << "1. 单生产者 + 单消费者: ";
    double spsc_avg = 0;
    for (int i = 0; i < TEST_ROUNDS; ++i)
    {
        spsc_avg += test_single_prod_single_cons<QueueType>();
    }
    spsc_avg /= TEST_ROUNDS;
    std::cout << std::fixed << std::setprecision(2) << spsc_avg / 1000000 << " Mops/sec" << std::endl;

    // 测试2：多生产者多消费者（线程数递增）
    std::cout << "\n2. 多生产者 + 多消费者 (线程数从" << MIN_THREADS << "到" << MAX_THREADS << "):" << std::endl;
    for (int threads = MIN_THREADS; threads <= MAX_THREADS; ++threads)
    {
        std::cout << "  线程数=" << threads << ": ";
        double mpmc_avg = 0;
        for (int i = 0; i < TEST_ROUNDS; ++i)
        {
            mpmc_avg += test_multi_prod_multi_cons<QueueType>(threads);
        }
        mpmc_avg /= TEST_ROUNDS;
        std::cout << std::fixed << std::setprecision(2) << mpmc_avg / 1000000 << " Mops/sec" << std::endl;
    }

    // 测试3：混合模式（每个线程既生产也消费）
    std::cout << "\n3. 混合模式 (线程数从" << MIN_THREADS << "到" << MAX_THREADS << "):" << std::endl;
    for (int threads = MIN_THREADS; threads <= MAX_THREADS; ++threads)
    {
        std::cout << "  线程数=" << threads << ": ";
        double mixed_avg = 0;
        for (int i = 0; i < TEST_ROUNDS; ++i)
        {
            mixed_avg += test_mixed_mode<QueueType>(threads);
        }
        mixed_avg /= TEST_ROUNDS;
        std::cout << std::fixed << std::setprecision(2) << mixed_avg / 1000000 << " Mops/sec" << std::endl;
    }

    return 0;
}