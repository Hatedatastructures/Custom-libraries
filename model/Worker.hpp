#include ".Task.hpp"
#include "./Cohort.hpp"
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
namespace _implemented_internally
{
  namespace _worker_structure{}
}
namespace _implemented_internally::_worker_structure
{
  class worker;
  /**
   * @enum worker_state
   * @brief 工作线程状态枚举
   *
   * 定义工作线程的各种状态，用于状态管理和监控
   */
  enum class worker_state
  {
    idle,     // 空闲状态 - 等待任务
    running,  // 运行状态 - 正在执行任务
    stopping, // 停止中   - 正在停止但未完全停止
    stopped,  // 已停止   - 线程已结束
    error     // 错误状态 - 发生异常
  };

  /**
   * @struct worker_statistics
   * @brief 工作线程统计信息
   *
   * 记录工作线程的性能统计数据，用于监控和优化
   */
  class worker_statistics
  {
  public:
    std::atomic<std::uint64_t> tasks_failed{0};           ///< 执行失败任务数量
    std::atomic<std::uint64_t> tasks_executed{0};         ///< 已执行任务数量
    std::atomic<std::uint64_t> total_idle_time{0};        ///< 总空闲时间(微秒)
    std::atomic<std::uint64_t> total_execution_time{0};   ///< 总执行时间(微秒)

    std::chrono::steady_clock::time_point start_time;     ///< 线程启动时间
    std::chrono::steady_clock::time_point last_task_time; ///< 最后任务执行时间

    worker_statistics()
    {
      reset();
    }

    /**
     * @brief 重置统计信息
     */
    void reset()
    {
      tasks_failed.store(0, std::memory_order_relaxed);
      tasks_executed.store(0, std::memory_order_relaxed);
      total_idle_time.store(0, std::memory_order_relaxed);
      total_execution_time.store(0, std::memory_order_relaxed);
      start_time = std::chrono::steady_clock::now();
      last_task_time = start_time;
    }

    /**
     * @brief 获取平均任务执行时间
     * @return 平均执行时间(微秒)
     */
    double get_average_execution_time() const
    {
      auto executed = tasks_executed.load(std::memory_order_relaxed);
      if (executed == 0)
        return 0.0;

      auto total_time = total_execution_time.load(std::memory_order_relaxed);
      return static_cast<double>(total_time) / executed;
    }

    /**
     * @brief 获取任务成功率
     * @return 成功率(0.0-1.0)
     */
    double get_success_rate() const
    {
      auto executed = tasks_executed.load(std::memory_order_relaxed);
      if (executed == 0)
        return 1.0;

      auto failed = tasks_failed.load(std::memory_order_relaxed);
      return static_cast<double>(executed - failed) / executed;
    }

    /**
     * @brief 获取线程利用率
     * @return 利用率(0.0-1.0)
     */
    double get_utilization() const
    {
      auto now = std::chrono::steady_clock::now();
      auto total_time = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time).count();

      if (total_time == 0)
        return 0.0;

      auto execution_time = total_execution_time.load(std::memory_order_relaxed);
      return static_cast<double>(execution_time) / total_time;
    }
  };
   /**
   * @class base_worker
   * @brief 工作线程基类
   *
   * 定义工作线程的基本接口和行为，所有具体的工作线程类型都继承自此类
   *
   * 设计模式： 模板方法模式：定义线程执行流程，策略模式：支持不同的任务获取策略
   *
   * 调用关系：被`thread_pool`管理和调用， 从`task_queue`获取任务， 执行`base_task`及其派生类
   */
  class worker_base
  {
  protected:
    std::unique_ptr<std::jthread> worker_thread; ///< 线程对象

    std::atomic<bool> stop{false}; ///< 停止标志
    std::atomic<bool> detached{false}; ///< 分离标志
    std::atomic<worker_state> state{worker_state::idle}; ///< 状态标志

    std::string worker_name; ///< 线程名称
    worker_statistics statistics; ///< 统计信息
    
    std::shared_ptr<_implemented_internally::structure_cohort::cohort_base> task_queue; ///< 任务队列

    std::function<void(const std::string&, _interior_task_ptr&)> start_callback;  ///< 任务开始回调
    std::function<void(const std::string&, _interior_task_ptr&)> finish_callback; ///< 任务完成回调
    std::function<void(const std::string&, const std::exception&)> abnormal_callback; ///< 任务异常回调 
  public:
  };
}