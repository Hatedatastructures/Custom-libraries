// #include "Thread_pool.hpp"
// #include <Windows.h>
// #include <memory>
// int main()
// {
//   auto thread_pool_test = con::make_high_performance_pool(32);
//   auto value =  thread_pool_test->get_config();
//   thread_pool_test->start();
//   auto func = [](std::string s)
//   {
//     std::cout << s << std::endl;
//     return std::string("执行完毕！");
//   };
//   auto func_first = []()
//   {
//     std::cout << "first" << std::endl;
//     Sleep(50000);
//     return std::string("first执行完毕!");
//   };
//   auto return_value = thread_pool_test->submit(func, "hello world");
//   auto return_value_first = thread_pool_test->submit(func_first);
//   std::cout << return_value.get() << std::endl;
//   std::cout << return_value_first.get() << std::endl;

//   auto message = thread_pool_test->get_performance_report();
//   std::cout << thread_pool_test->auto_repair() << std::endl;
//   std::cout << message << std::endl;
//   return 0;
// }
// //642行性能分析未处理
#include <future>
#include <chrono>
#include <memory>
#include "Unit.hpp"
#include "Rank.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <numeric>
#include <iomanip>
#include <algorithm>
#include <atomic>
#include <cmath>
#include "Integration.hpp"
using namespace internals::structure_u;
using namespace internals::structure_r;
using namespace std::chrono;
class internal_future
{
public:
  template <class deduction_t>
  internal_future(std::future<deduction_t> f)
      : _ptr(std::make_shared<deduction_model<deduction_t>>(std::move(f))) {}

  void wait() const { _ptr->wait(); }
  bool valid() const { return _ptr->valid(); }
  template <class convert_t>
  convert_t get() const
  {
    auto derived_ptr = std::dynamic_pointer_cast<deduction_model<convert_t>>(_ptr);
    if (!derived_ptr)
    {
      throw execution_exception("类型转换失败: 内部类型不匹配", 0);
    }
    return derived_ptr->get();
  }

private:
  struct concepts
  {
    virtual ~concepts() = default;
    virtual void wait() const = 0;
    virtual bool valid() const = 0;
  };
  template <class deduction_t>
  struct deduction_model final : concepts
  {
    explicit deduction_model(std::future<deduction_t> f) : _fut(std::move(f)) {}
    void wait() const override { _fut.wait(); }
    bool valid() const override { return _fut.valid(); }
    deduction_t get() const { return _fut.get(); }
    mutable std::future<deduction_t> _fut;
  };
  std::shared_ptr<concepts> _ptr;
};

namespace rank_test
{
  using namespace pool;
  using namespace internals::structure_r;
  using namespace std::chrono;

  // 测试配置参数
  constexpr size_t BASELINE_TASKS = 100000;      // 基准任务数量
  constexpr size_t CONCURRENT_THREADS = 12;      // 并发线程数
  constexpr size_t DEPENDENCY_CHAIN_LENGTH = 50; // 依赖链长度
  constexpr int TEST_ROUNDS = 10;                // 测试轮次

  // 简单任务函数（无返回值）
  void simple_task()
  {
    // 模拟微小计算量
    volatile int sum = 0;
    for (int i = 0; i < 100; ++i)
      sum += i;
  }

  // 有返回值的任务函数
  int result_task(int x)
  {
    volatile int sum = x;
    for (int i = 0; i < 500; ++i)
      sum *= (i + 1);
    return sum;
  }

  // 超时任务回调
  void timeout_handler()
  {
    // 空回调，仅用于测试
  }

  // 计算平均值和标准差
  std::pair<double, double> calculate_stats(const std::vector<long long> &data)
  {
    if (data.empty())
      return {0, 0};

    double mean = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
    double variance = 0;
    for (auto val : data)
    {
      variance += std::pow(val - mean, 2);
    }
    variance /= data.size();
    return {mean, std::sqrt(variance)};
  }

  // 测试1：基准任务性能测试
  void test_baseline_performance()
  {
    std::cout << "\n=== 基准任务性能测试 ===" << std::endl;
    std::vector<long long> durations;

    for (int round = 0; round < TEST_ROUNDS; ++round)
    {
      // 每轮测试创建新的队列实例，避免重用已关闭的队列
      rank_standard queue(100000);
      auto start = high_resolution_clock::now();

      // 提交任务
      for (size_t i = 0; i < BASELINE_TASKS; ++i)
      {
        auto task = make_unit_standard(simple_task, "baseline_task_" + std::to_string(i));
        queue.push(task);
      }

      // 消费任务（模拟工作线程）
      std::thread consumer([&queue]()
                           {
            while (!queue.empty() || !queue.closed()) {
                if (auto task = queue.try_pop_for(std::chrono::seconds(10))) {
                    task->execute();
                } else if (queue.closed()) {
                    // 队列已关闭，尝试处理剩余任务
                    while (auto remaining_task = queue.try_pop()) {
                        remaining_task->execute();
                    }
                    break;
                }
            } });

      // 等待队列处理完成
      while (!queue.empty())
      {
        std::this_thread::yield();
      }
      queue.close();
      consumer.join();

      auto end = high_resolution_clock::now();
      long long duration = duration_cast<milliseconds>(end - start).count();
      durations.push_back(duration);

      std::cout << "轮次 " << round + 1 << ": "
                << duration << "ms, "
                << (BASELINE_TASKS * 1000 / duration) << " 任务/秒" << std::endl;
    }

    auto [mean, stddev] = calculate_stats(durations);
    std::cout << "平均值: " << mean << "ms, 标准差: " << stddev << "ms" << std::endl;
    std::cout << "平均吞吐量: " << (BASELINE_TASKS * 1000 / mean) << " 任务/秒" << std::endl;
  }

  // 测试2：并发提交性能测试
  void test_concurrent_submission()
  {
    std::cout << "\n=== 并发提交性能测试 ===" << std::endl;
    std::vector<long long> durations;

    for (int round = 0; round < TEST_ROUNDS; ++round)
    {
      // 每轮测试创建新的队列实例，避免重用已关闭的队列
      rank_standard queue(100000);
      auto start = high_resolution_clock::now();
      std::vector<std::thread> submitters;
      size_t tasks_per_thread = BASELINE_TASKS / CONCURRENT_THREADS;

      // 并发提交任务
      for (size_t t = 0; t < CONCURRENT_THREADS; ++t)
      {
        submitters.emplace_back([&, t]()
                                {
                for (size_t i = 0; i < tasks_per_thread; ++i) {
                    auto task = make_unit_standard(simple_task, 
                        "concurrent_task_" + std::to_string(t) + "_" + std::to_string(i));
                    queue.push(task, backpressure::block);
                } });
      }

      // 等待所有提交完成
      for (auto &th : submitters)
      {
        th.join();
      }

      // 消费任务
      std::thread consumer([&queue]()
                           {
            while (!queue.empty() || !queue.closed()) {
                if (auto task = queue.try_pop_for(std::chrono::seconds(10))) {
                    task->execute();
                } else if (queue.closed()) {
                    // 队列已关闭，尝试处理剩余任务
                    while (auto remaining_task = queue.try_pop()) {
                        remaining_task->execute();
                    }
                    break;
                }
            } });

      // 等待处理完成
      while (!queue.empty())
      {
        std::this_thread::yield();
      }

      // 等待一小段时间确保所有任务都被处理
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      queue.close();
      consumer.join();

      auto end = high_resolution_clock::now();
      long long duration = duration_cast<milliseconds>(end - start).count();
      durations.push_back(duration);

      std::cout << "轮次 " << round + 1 << ": "
                << duration << "ms, "
                << (BASELINE_TASKS * 1000 / duration) << " 任务/秒" << std::endl;
    }

    auto [mean, stddev] = calculate_stats(durations);
    std::cout << "平均值: " << mean << "ms, 标准差: " << stddev << "ms" << std::endl;
    std::cout << "并发吞吐量: " << (BASELINE_TASKS * 1000 / mean) << " 任务/秒" << std::endl;
  }

  // // 测试3：带返回值的任务性能
  // void test_result_tasks()
  // {
  //   std::cout << "\n=== 带返回值任务性能测试 ===" << std::endl;
  //   rank_standard queue(10000);
  //   std::vector<long long> durations;

  //   for (int round = 0; round < TEST_ROUNDS; ++round)
  //   {
  //     auto start = high_resolution_clock::now();
  //     std::vector<std::shared_ptr<unit_ordinary>> tasks;

  //     // 创建带返回值的任务
  //     for (size_t i = 0; i < BASELINE_TASKS / 10; ++i)
  //     { // 数量减少，因为计算量更大
  //       auto task = make_unit_standard([i]()
  //                                      { return result_task(i); },
  //                                      "result_task_" + std::to_string(i));
  //       tasks.push_back(task);
  //       queue.push(task);
  //     }

  //     // 消费任务并获取结果
  //     std::thread consumer([&queue]()
  //                          {
  //           while (!queue.empty() || !queue.closed()) {
  //               if (auto task = queue.try_pop()) {
  //                   task->execute();
  //               }
  //           } });

  //     // 等待处理完成并验证结果
  //     for (auto &task : tasks)
  //     {
  //       task->wait();
  //       task->get_result(); // 实际使用中会处理结果
  //     }

  //     queue.close();
  //     consumer.join();

  //     auto end = high_resolution_clock::now();
  //     long long duration = duration_cast<milliseconds>(end - start).count();
  //     durations.push_back(duration);

  //     std::cout << "轮次 " << round + 1 << ": "
  //               << duration << "ms, "
  //               << ((BASELINE_TASKS / 10) * 1000 / duration) << " 任务/秒" << std::endl;
  //   }

  //   auto [mean, stddev] = calculate_stats(durations);
  //   std::cout << "平均值: " << mean << "ms, 标准差: " << stddev << "ms" << std::endl;
  // }

  // 测试4：依赖任务链性能
  void test_dependency_chain()
  {
    std::cout << "\n=== 依赖任务链性能测试 ===" << std::endl;
    std::vector<long long> durations;

    for (int round = 0; round < TEST_ROUNDS; ++round)
    {
      // 每轮测试创建新的队列实例，避免重用已关闭的队列
      rank_standard queue(1000);
      auto start = high_resolution_clock::now();

      // 创建依赖链：task1 -> task2 -> ... -> taskN
      std::shared_ptr<unit_ordinary> prev_task;
      for (size_t i = 0; i < DEPENDENCY_CHAIN_LENGTH; ++i)
      {
        auto task = make_unit_reliance([i]()
                                       {
                                         volatile int sum = 0;
                                         for (int j = 0; j < 100000; ++j)
                                           sum += j; // 增加计算量
                                       },
                                       prev_task, "dependency_task_" + std::to_string(i));

        queue.push(task);
        prev_task = task;
      }

      // 处理任务
      std::thread consumer([&queue]()
                           {
            while (!queue.empty() || !queue.closed()) {
                if (auto task = queue.try_pop_for(std::chrono::seconds(10))) {
                    // 检查是否是依赖任务并等待依赖满足
                    if (auto dep_task = std::dynamic_pointer_cast<unit_reliance<decltype(&simple_task)>>(task)) {
                        while (!dep_task->are_dependencies_satisfied()) {
                            std::this_thread::sleep_for(microseconds(10));
                        }
                    }
                    task->execute();
                } else if (queue.closed()) {
                    // 队列已关闭，尝试处理剩余任务
                    while (auto remaining_task = queue.try_pop()) {
                        remaining_task->execute();
                    }
                    break;
                }
            } });

      // 等待最终任务完成
      if (prev_task)
      {
        prev_task->wait();
      }

      // 等待一小段时间确保所有任务都被处理
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      queue.close();
      consumer.join();

      auto end = high_resolution_clock::now();
      long long duration = duration_cast<milliseconds>(end - start).count();
      durations.push_back(duration);

      std::cout << "轮次 " << round + 1 << ": 链长 " << DEPENDENCY_CHAIN_LENGTH
                << ", 耗时 " << duration << "ms" << std::endl;
    }

    auto [mean, stddev] = calculate_stats(durations);
    std::cout << "平均链耗时: " << mean << "ms, 标准差: " << stddev << "ms" << std::endl;
  }

  // 测试5：队列压力测试（极限负载）
  void test_queue_stress()
  {
    std::cout << "\n=== 队列压力测试 ===" << std::endl;
    const size_t stress_tasks = BASELINE_TASKS * 5;
    const size_t producer_count = CONCURRENT_THREADS;
    const size_t consumer_count = CONCURRENT_THREADS / 2;
    std::vector<long long> durations;

    std::cout << "测试配置: 总任务数=" << stress_tasks
              << ", 生产者线程=" << producer_count
              << ", 消费者线程=" << consumer_count << std::endl;

    for (int round = 0; round < TEST_ROUNDS; ++round)
    {
      // 每轮测试创建新的队列实例，避免重用已关闭的队列
      rank_standard queue(BASELINE_TASKS / 2);

      auto start = high_resolution_clock::now();
      std::atomic<size_t> completed_tasks{0};       // 原子计数器跟踪完成的任务
      std::atomic<size_t> total_produced{0};        // 跟踪实际生产的任务数
      std::atomic<bool> production_finished{false}; // 生产完成标志

      // 多个生产者线程
      std::vector<std::thread> producers;
      producers.reserve(producer_count);

      // 计算每个生产者的任务量，处理整除余数
      const size_t base_tasks_per_producer = stress_tasks / producer_count;
      const size_t remaining_tasks = stress_tasks % producer_count;

      for (size_t t = 0; t < producer_count; ++t)
      {
        // 为每个生产者分配任务量，最后几个处理余数
        const size_t tasks_to_produce = base_tasks_per_producer + (t < remaining_tasks ? 1 : 0);

        producers.emplace_back([&, t, tasks_to_produce]()
                               {
                for (size_t i = 0; i < tasks_to_produce; ++i) {
                    // 任务ID：线程号_序号，确保唯一
                    const std::string task_id = "stress_task_" + std::to_string(t) + "_" + std::to_string(i);
                    
                    // 创建任务：包含固定处理时间和完成计数
                    auto task = make_unit_standard([&]() {
                        std::this_thread::sleep_for(microseconds(10));  // 模拟处理耗时
                        completed_tasks.fetch_add(1, std::memory_order_relaxed);
                    }, task_id);
                    
                    // 推送任务，队列满时阻塞
                    if (queue.push(task, backpressure::block)) {
                        total_produced.fetch_add(1, std::memory_order_relaxed);
                    }
                } });
      }

      // 多个消费者线程
      std::vector<std::thread> consumers;
      consumers.reserve(consumer_count);
      std::atomic<bool> should_stop{false};

      for (size_t t = 0; t < consumer_count; ++t)
      {
        consumers.emplace_back([&]()
                               {
                // 改进的消费者逻辑：继续处理直到生产完成且队列为空
                while (!should_stop.load(std::memory_order_acquire)) {
                    if (auto task = queue.try_pop_for(std::chrono::seconds(10))) {
                        task->execute();  // 执行任务
                    } else {
                        // 如果生产已完成且队列为空，则退出
                        if (production_finished.load(std::memory_order_acquire) && queue.empty()) {
                            break;
                        }
                        // 检查队列是否已关闭
                        if (queue.closed()) {
                            // 队列已关闭，尝试处理剩余任务
                            while (auto remaining_task = queue.try_pop()) {
                                remaining_task->execute();
                            }
                            break;
                        }
                    }
                } });
      }

      // 等待所有生产者完成任务提交
      for (auto &th : producers)
      {
        if (th.joinable())
        {
          th.join();
        }
      }

      // 标记生产完成
      production_finished.store(true, std::memory_order_release);

      // 等待一小段时间确保所有任务都被处理
      std::this_thread::sleep_for(std::chrono::milliseconds(100));

      // 等待所有任务执行完成（带超时保护）
      const auto wait_start = high_resolution_clock::now();
      bool all_completed = false;
      while (completed_tasks.load(std::memory_order_acquire) < stress_tasks)
      {
        std::this_thread::yield();

        // 超时保护：防止永久阻塞
        const auto elapsed = duration_cast<milliseconds>(
                                 high_resolution_clock::now() - wait_start)
                                 .count();
        if (elapsed > 10000)
        { // 10秒超时
          std::cerr << "警告：任务处理超时，可能存在死锁或未完成的任务" << std::endl;
          break;
        }
      }
      all_completed = (completed_tasks.load() == stress_tasks);

      // 通知消费者停止并等待退出
      should_stop.store(true, std::memory_order_release);
      queue.close(); // 关闭队列以唤醒等待的消费者

      for (auto &th : consumers)
      {
        if (th.joinable())
        {
          th.join();
        }
      }

      // 计算耗时和吞吐量
      auto end = high_resolution_clock::now();
      long long duration = duration_cast<milliseconds>(end - start).count();
      durations.push_back(duration);

      // 输出本轮详细信息
      std::cout << "轮次 " << round + 1 << ": "
                << "总任务=" << stress_tasks << ", "
                << "完成=" << completed_tasks.load() << ", "
                << "耗时=" << duration << "ms, "
                << "吞吐量=" << (stress_tasks * 1000.0 / duration) << " 任务/秒"
                << (all_completed ? "" : " [未完成]") << std::endl;
    }

    // 计算并输出统计结果
    auto [mean, stddev] = calculate_stats(durations);
    std::cout << "统计结果: "
              << "平均耗时=" << mean << "ms, "
              << "标准差=" << stddev << "ms, "
              << "平均吞吐量=" << (stress_tasks * 1000.0 / mean) << " 任务/秒" << std::endl;
  }
}
namespace pro_test
{
  struct TestConfig
  {
    size_t total_tasks = 1000000;                  // 总任务数
    size_t producer_count = 4;                    // 生产者线程数
    size_t consumer_count = 4;                    // 消费者线程数
    size_t queue_max_size = 100000;                // 队列最大容量
    backpressure push_mode = backpressure::block; // 推送模式
    int test_duration_sec = 100;                   // 测试持续时间(秒)
  };

  // 测试结果
  struct TestResult
  {
    size_t total_produced = 0;         // 总生产任务数
    size_t total_consumed = 0;         // 总消费任务数
    size_t lost_tasks = 0;             // 丢失的任务数
    double avg_produce_time = 0;       // 平均生产时间(微秒)
    double avg_consume_time = 0;       // 平均消费时间(微秒)
    double throughput = 0;             // 吞吐量(任务/秒)
    system_clock::duration total_time; // 总耗时
  };

  // 测试任务单元
  class TestUnit : public unit_ordinary
  {
  private:
    size_t _id;
    system_clock::time_point _produce_time;

  public:
    TestUnit(size_t id) : unit_ordinary([](){}, "TestUnit"), _id(id) {}

    derivation execute() override
    {
      // 模拟任务处理时间
      std::this_thread::sleep_for(microseconds(1));
      return derivation();
    }

    size_t get_id() const { return _id; }
    void set_produce_time() { _produce_time = system_clock::now(); }
    system_clock::time_point get_produce_time() const { return _produce_time; }
  };

  // 压力测试函数
  template <typename QueueType>
  TestResult stress_test(const TestConfig &config)
  {
    TestResult result;
    auto start_time = system_clock::now();

    // 创建队列
    QueueType queue(config.queue_max_size);

    // 原子变量用于同步和计数
    std::atomic<size_t> task_counter(0);
    std::atomic<size_t> produced_counter(0);
    std::atomic<size_t> consumed_counter(0);
    std::atomic<bool> stop_flag(false);
    std::atomic<size_t> lost_counter(0);

    // 存储生产和消费时间用于计算平均值
    std::vector<long long> produce_times;
    std::vector<long long> consume_times;
    std::mutex time_mutex;

    // 生产者线程函数
    auto producer_func = [&]()
    {
      while (!stop_flag)
      {
        size_t task_id = task_counter.fetch_add(1, std::memory_order_relaxed);
        if (task_id >= config.total_tasks)
        {
          break;
        }

        auto task = std::make_shared<TestUnit>(task_id);
        auto start = system_clock::now();

        bool pushed = queue.push(task, config.push_mode);
        auto end = system_clock::now();

        if (pushed)
        {
          task->set_produce_time();
          produced_counter++;
          std::lock_guard<std::mutex> lock(time_mutex);
          produce_times.push_back(duration_cast<microseconds>(end - start).count());
        }
        else
        {
          lost_counter++;
        }
      }
    };

    // 消费者线程函数
    auto consumer_func = [&]()
    {
      while (!stop_flag)
      {
        auto start = system_clock::now();
        auto task = queue.pop();
        auto end = system_clock::now();

        if (!task)
        {
          // 队列已关闭且为空
          if (queue.closed())
          {
            // 队列已关闭，尝试处理剩余任务
            while (auto remaining_task = queue.try_pop())
            {
              remaining_task->execute();
              consumed_counter++;
            }
            break;
          }
          continue;
        }

        // 处理任务
        task->execute();

        std::lock_guard<std::mutex> lock(time_mutex);
        consume_times.push_back(duration_cast<microseconds>(end - start).count());
        consumed_counter++;
      }
    };

    // 创建并启动生产者线程
    std::vector<std::thread> producers;
    for (size_t i = 0; i < config.producer_count; ++i)
    {
      producers.emplace_back(producer_func);
    }

    // 创建并启动消费者线程
    std::vector<std::thread> consumers;
    for (size_t i = 0; i < config.consumer_count; ++i)
    {
      consumers.emplace_back(consumer_func);
    }

    // 等待测试完成（达到任务数或超时）
    auto wait_start = system_clock::now();
    while (true)
    {
      if (consumed_counter >= config.total_tasks ||
          duration_cast<seconds>(system_clock::now() - wait_start).count() > config.test_duration_sec)
      {
        stop_flag = true;
        break;
      }
      std::this_thread::sleep_for(milliseconds(100));
    }

    // 关闭队列，确保消费者能退出
    queue.close();

    // 等待所有线程结束
    for (auto &t : producers)
    {
      if (t.joinable())
        t.join();
    }
    for (auto &t : consumers)
    {
      if (t.joinable())
        t.join();
    }

    // 计算测试结果
    result.total_time = system_clock::now() - start_time;
    result.total_produced = produced_counter;
    result.total_consumed = consumed_counter;
    result.lost_tasks = lost_counter;

    // 计算平均生产时间
    if (!produce_times.empty())
    {
      long long total = std::accumulate(produce_times.begin(), produce_times.end(), 0LL);
      result.avg_produce_time = static_cast<double>(total) / produce_times.size();
    }

    // 计算平均消费时间
    if (!consume_times.empty())
    {
      long long total = std::accumulate(consume_times.begin(), consume_times.end(), 0LL);
      result.avg_consume_time = static_cast<double>(total) / consume_times.size();
    }

    // 计算吞吐量（任务/秒）
    double seconds = duration_cast<duration<double>>(result.total_time).count();
    result.throughput = result.total_consumed / seconds;

    return result;
  }

  // 打印测试结果
  void print_result(const std::string &queue_type, const TestConfig &config, const TestResult &result)
  {
    std::cout << "=== " << queue_type << " 压力测试结果 ===" << std::endl;
    std::cout << "测试配置:" << std::endl;
    std::cout << "  总任务数: " << config.total_tasks << std::endl;
    std::cout << "  生产者线程数: " << config.producer_count << std::endl;
    std::cout << "  消费者线程数: " << config.consumer_count << std::endl;
    std::cout << "  队列最大容量: " << config.queue_max_size << std::endl;
    std::cout << "  推送模式: " << static_cast<int>(config.push_mode) << std::endl;

    std::cout << "\n测试结果:" << std::endl;
    std::cout << "  总耗时: " << duration_cast<milliseconds>(result.total_time).count() << "ms" << std::endl;
    std::cout << "  总生产任务数: " << result.total_produced << std::endl;
    std::cout << "  总消费任务数: " << result.total_consumed << std::endl;
    std::cout << "  丢失任务数: " << result.lost_tasks << std::endl;
    std::cout << "  平均生产时间: " << std::fixed << std::setprecision(2) << result.avg_produce_time << "µs" << std::endl;
    std::cout << "  平均消费时间: " << std::fixed << std::setprecision(2) << result.avg_consume_time << "µs" << std::endl;
    std::cout << "  吞吐量: " << std::fixed << std::setprecision(2) << result.throughput << "任务/秒" << std::endl;
    std::cout << "  完整性: " << (result.total_consumed + result.lost_tasks == result.total_produced ? "OK" : "ERROR") << std::endl;
    std::cout << "======================================" << std::endl
              << std::endl;
  }
}
int main()
{
  // {
  //   internals::structure_u::unit_ordinary p([](){ std::cout << "hello,worrld!" << std::endl; },
  //    "High Priority Task", weight::high);
  //   std::cout << p.get_priority() << std::endl;
  //   p.execute();
  //   // std::cout << p.get_future().get() << std::endl;
  //   std::cout << "Task ID: " << p.get_identifier() << std::endl;
  //   std::cout << p.get_task_name() << std::endl;

  //   std::cout << "-----------------" << std::endl;

  //   internals::structure_u::unit_ordinary p2([](){ std::cout << "stream!" << std::endl; },
  //   "Higher Priority Task", weight::highest);

  //   std::cout << (p2 < p) << std::endl; // false
  //   std::cout << (p2 > p) << std::endl; // true
  //   p2.execute();
  //   // std::cout << p2.get_future().get() << std::endl;
  //   std::cout << "Task ID: " << p2.get_identifier() << std::endl;
  //   std::cout << p2.get_task_name() << std::endl;

  // }
  // {
  //   // 测试返回值自动推导
  //   internal_future fut1(std::async(std::launch::async, []() { return 42; }));
  //   auto result = fut1.get<int>();
  //   std::cout << "Result: " << result << std::endl; // 输出: Result: 42
  // }
  // {
  //   // 测试返回值类
  //   internals::structure_u::unit_standard<std::function<int()>, int> task
  //   ([]()-> int { std::cout << "当前是一个带返回值的标准任务"<< std::endl; return 100; }, "Return Value Task");
  //   auto value = task.execute().get<int>();
  //   std::cout << "Value from task: " << value << std::endl;
  //   auto res = task.get_future();
  //   std::cout << "Result: " << res.get() << std::endl;
  //   std::cout << "Task Name: " << task.get_task_name() << std::endl;
  //   std::cout << "Task ID: " << task.get_identifier() << std::endl;
  //   // std::cout << "Is Void Task: " << std::boolalpha << task.is_void_task() << std::endl;
  // }
  // {
  //   auto task = internals::structure_u::make_unit_standard
  //   ([]()-> int { std::cout << "当前是一个带返回值的标准任务"<< std::endl; return 100; }, "Return Value Task");
  //   auto value = task->execute().get<int>();
  //   std::cout << "Value from task: " << value << std::endl;
  //   auto res = task->get_future();
  //   std::cout << "Result: " << res.get() << std::endl;
  //   std::cout << "Task Name: " << task->get_task_name() << std::endl;
  //   std::cout << "Task ID: " << task->get_identifier() << std::endl;
  //   // std::cout << "Is Void Task: " << std::boolalpha << task.is_void_task() << std::endl;
  //   std::cout << "-----------------" << std::endl;

  //   auto ptrs = internals::structure_u::make_unit_reliance<300ULL>
  //   ([]()-> int { std::cout << "当前是一个依赖任务"<< std::endl; return 200; },task,
  //    "Reliance Task");
  //   auto ptrs_value = ptrs->execute().get<int>();
  //   std::cout << "Value from task: " << ptrs_value << std::endl;
  //   auto ptrs_res = ptrs->get_future();
  //   std::cout << "Result: " << ptrs_res.get() << std::endl;
  //   std::cout << "Task Name: " << ptrs->get_task_name() << std::endl;
  //   std::cout << "Task ID: " << ptrs->get_identifier() << std::endl;
  //   // std::cout << "Is Void Task: " << std::boolalpha << ptr->is_void_task() << std::endl;
  // }
  // {
  //   // convert_time ct;
  //   auto res = convert_time::to_seconds(std::chrono::minutes(5));
  //   std::cout << res.count() << std::endl; // 输出: 300
  // }
  // {
  //   // internals::structure_r::rank_standard rank;
  //   std::cout << "===== 任务调度系统性能测试 =====" << std::endl;
  //   std::cout << "测试配置: 基准任务数=" << rank_test::BASELINE_TASKS
  //             << ", 并发线程数=" << rank_test::CONCURRENT_THREADS
  //             << ", 测试轮次=" << rank_test::TEST_ROUNDS << std::endl;

  //   rank_test::test_baseline_performance();
  //   rank_test::test_concurrent_submission();
  //   // rank_test::test_result_tasks();
  //   rank_test::test_dependency_chain();
  //   rank_test::test_queue_stress();

  //   std::cout << "\n===== 性能测试完成 =====" << std::endl;
  // }
  {
    // pro_test::TestConfig config;
    // config.total_tasks = 10000000;          // 1000万任务
    // config.producer_count = 16;             // 16个生产者
    // config.consumer_count = 16;             // 16个消费者
    // config.queue_max_size = 1000;           // 队列最大1000容量
    // config.push_mode = backpressure::block; // 阻塞模式
    // config.test_duration_sec = 120;         // 最长120秒

    // // 测试标准队列
    // auto standard_result = pro_test::stress_test<rank_standard>(config);
    // pro_test::print_result("标准队列(rank_standard)", config, standard_result);

    // // 测试优先级队列
    // auto priority_result = pro_test::stress_test<rank_priority>(config);
    // pro_test::print_result("优先级队列(rank_priority)", config, priority_result);

    // 测试不同背压策略（可选）
    /*
    config.push_mode = backpressure::overwrite;
    auto overwrite_result = stress_test<rank_standard>(config);
    print_result("标准队列(overwrite模式)", config, overwrite_result);

    config.push_mode = backpressure::drop;
    auto drop_result = stress_test<rank_standard>(config);
    print_result("标准队列(drop模式)", config, drop_result);
    */
  }
  {
    std::jthread test([](){std::cout << std::this_thread::get_id() << std::endl;});
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::jthread test2([](){std::cout << std::this_thread::get_id() << std::endl;});
    test.join();
    test2.join();
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
  return 0;
}