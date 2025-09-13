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
  constexpr int TEST_ROUNDS = 100;               // 测试轮次

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
    rank_standard queue(100000);
    std::vector<long long> durations;

    for (int round = 0; round < TEST_ROUNDS; ++round)
    {
      auto start = high_resolution_clock::now();

      // 提交任务
      for (size_t i = 0; i < BASELINE_TASKS; ++i)
      {
        auto task = make_uint_standard(simple_task, "baseline_task_" + std::to_string(i));
        queue.push(task);
      }

      // 消费任务（模拟工作线程）
      std::thread consumer([&queue]()
                           {
            while (!queue.empty() || !queue.closed()) {
                if (auto task = queue.try_pop()) {
                    task->execute();
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
    rank_standard queue(100000);
    std::vector<long long> durations;

    for (int round = 0; round < TEST_ROUNDS; ++round)
    {
      auto start = high_resolution_clock::now();
      std::vector<std::thread> submitters;
      size_t tasks_per_thread = BASELINE_TASKS / CONCURRENT_THREADS;

      // 并发提交任务
      for (size_t t = 0; t < CONCURRENT_THREADS; ++t)
      {
        submitters.emplace_back([&, t]()
                                {
                for (size_t i = 0; i < tasks_per_thread; ++i) {
                    auto task = make_uint_standard(simple_task, 
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
                if (auto task = queue.try_pop()) {
                    task->execute();
                }
            } });

      // 等待处理完成
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
  //       auto task = make_uint_standard([i]()
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
      auto start = high_resolution_clock::now();
      rank_standard queue(1000);

      // 创建依赖链：task1 -> task2 -> ... -> taskN
      std::shared_ptr<unit_ordinary> prev_task;
      for (size_t i = 0; i < DEPENDENCY_CHAIN_LENGTH; ++i)
      {
        auto task = make_uint_reliance([i]()
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
                if (auto task = queue.try_pop()) {
                    // 检查是否是依赖任务并等待依赖满足
                    if (auto dep_task = std::dynamic_pointer_cast<unit_reliance<decltype(&simple_task)>>(task)) {
                        while (!dep_task->are_dependencies_satisfied()) {
                            std::this_thread::sleep_for(microseconds(10));
                        }
                    }
                    task->execute();
                }
            } });

      // 等待最终任务完成
      if (prev_task)
      {
        prev_task->wait();
      }

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

    // 队列容量为任务数的一半，制造背压场景
    rank_standard queue(BASELINE_TASKS / 2);
    std::vector<long long> durations;

    std::cout << "测试配置: 总任务数=" << stress_tasks
              << ", 生产者线程=" << producer_count
              << ", 消费者线程=" << consumer_count << std::endl;

    for (int round = 0; round < TEST_ROUNDS; ++round)
    {
      auto start = high_resolution_clock::now();
      std::atomic<size_t> completed_tasks{0}; // 原子计数器跟踪完成的任务
      std::atomic<size_t> total_produced{0};  // 跟踪实际生产的任务数

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
                    auto task = make_uint_standard([&]() {
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
      for (size_t t = 0; t < consumer_count; ++t)
      {
        consumers.emplace_back([&queue]()
                               {
                // 循环消费直到队列关闭且为空
                while (!queue.closed() || !queue.empty()) {
                    // 尝试弹出任务，超时100ms避免无限等待
                    if (auto task = queue.try_pop_for(milliseconds(100))) {
                        task->execute();  // 执行任务
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

      // 等待所有任务执行完成（带超时保护）
      const auto wait_start = high_resolution_clock::now();
      bool all_completed = false;
      while (completed_tasks.load(std::memory_order_acquire) < stress_tasks)
      {
        std::this_thread::yield();

        // 超时保护：防止永久阻塞（5倍预期时间）
        const auto elapsed = duration_cast<milliseconds>(
                                 high_resolution_clock::now() - wait_start)
                                 .count();
        if (elapsed > 5000)
        { // 5秒超时
          std::cerr << "警告：任务处理超时，可能存在死锁或未完成的任务" << std::endl;
          break;
        }
      }
      all_completed = (completed_tasks == stress_tasks);

      // 关闭队列并等待消费者退出
      queue.close();
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
                << "完成=" << completed_tasks << ", "
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
  //   auto task = internals::structure_u::make_uint_standard
  //   ([]()-> int { std::cout << "当前是一个带返回值的标准任务"<< std::endl; return 100; }, "Return Value Task");
  //   auto value = task->execute().get<int>();
  //   std::cout << "Value from task: " << value << std::endl;
  //   auto res = task->get_future();
  //   std::cout << "Result: " << res.get() << std::endl;
  //   std::cout << "Task Name: " << task->get_task_name() << std::endl;
  //   std::cout << "Task ID: " << task->get_identifier() << std::endl;
  //   // std::cout << "Is Void Task: " << std::boolalpha << task.is_void_task() << std::endl;
  //   std::cout << "-----------------" << std::endl;

  //   auto ptrs = internals::structure_u::make_uint_reliance<300ULL>
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
  {
    // internals::structure_r::rank_standard rank;
    std::cout << "===== 任务调度系统性能测试 =====" << std::endl;
    std::cout << "测试配置: 基准任务数=" << rank_test::BASELINE_TASKS
              << ", 并发线程数=" << rank_test::CONCURRENT_THREADS
              << ", 测试轮次=" << rank_test::TEST_ROUNDS << std::endl;

    rank_test::test_baseline_performance();
    rank_test::test_concurrent_submission();
    // rank_test::test_result_tasks();
    rank_test::test_dependency_chain();
    rank_test::test_queue_stress();

    std::cout << "\n===== 性能测试完成 =====" << std::endl;
  }
  return 0;
}