#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>
#include <mutex>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <unistd.h>
#include <sys/resource.h>
#include <sys/sysinfo.h>
#endif

class load_metrics
{
private:
  // 用于内部计算的时间点
  std::chrono::steady_clock::time_point last_cpu_measurement;
  std::chrono::steady_clock::time_point last_throughput_measurement;
  std::chrono::steady_clock::time_point last_memory_measurement;

  // 用于计算CPU利用率的先前值
  std::chrono::steady_clock::time_point prev_cpu_time;
  std::size_t completed_tasks_since_last{0};

  // 内部状态跟踪
  std::size_t total_completed_tasks{0};

  // 减少系统调用频率的控制变量
  std::atomic<int> update_counter{0};

// Windows性能计数器句柄
#ifdef _WIN32
  PDH_HQUERY cpu_query{nullptr};
  PDH_HCOUNTER cpu_counter{nullptr};
#endif

  // 互斥锁保护资源密集型操作
  std::mutex update_mutex;

public:
  std::atomic<double> throughput{0.0};        // 吞吐量(任务/秒)
  std::atomic<double> memory_usage{0.0};      // 内存使用率
  std::atomic<double> cpu_utilization{0.0};   // CPU利用率
  std::atomic<double> average_task_time{0.0}; // 平均任务执行时间

  std::atomic<std::size_t> queue_length{0};   // 队列长度
  std::atomic<std::size_t> active_threads{0}; // 活跃线程数

  std::chrono::steady_clock::time_point last_update; // 最后更新时间

  load_metrics()
  {
    reset();

    // 初始化CPU测量
    prev_cpu_time = get_process_cpu_time();
    last_cpu_measurement = std::chrono::steady_clock::now();
    last_throughput_measurement = std::chrono::steady_clock::now();
    last_memory_measurement = std::chrono::steady_clock::now();

#ifdef _WIN32
    // 初始化Windows性能计数器
    PdhOpenQuery(nullptr, 0, &cpu_query);
    PdhAddEnglishCounter(cpu_query, "\\Processor(_Total)\\% Processor Time", 0, &cpu_counter);
    PdhCollectQueryData(cpu_query);
#endif
  }

  ~load_metrics()
  {
#ifdef _WIN32
    if (cpu_query)
    {
      PdhCloseQuery(cpu_query);
    }
#endif
  }

  /**
   * @brief 重置指标
   */
  void reset()
  {
    throughput.store(0.0, std::memory_order_relaxed);
    memory_usage.store(0.0, std::memory_order_relaxed);
    cpu_utilization.store(0.0, std::memory_order_relaxed);
    average_task_time.store(0.0, std::memory_order_relaxed);

    queue_length.store(0, std::memory_order_relaxed);
    active_threads.store(0, std::memory_order_relaxed);

    total_completed_tasks = 0;
    completed_tasks_since_last = 0;
    update_counter.store(0, std::memory_order_relaxed);

    last_update = std::chrono::steady_clock::now();
    last_cpu_measurement = last_update;
    last_throughput_measurement = last_update;
    last_memory_measurement = last_update;
  }

  /**
   * @brief 更新所有系统指标（轻量级版本，减少系统调用）
   */
  void update_metrics_light()
  {
    // 使用计数器减少系统调用频率
    int counter = update_counter.fetch_add(1, std::memory_order_relaxed);

    // 每10次调用更新一次内存和CPU（减少系统调用）
    if (counter % 10 == 0)
    {
      std::lock_guard<std::mutex> lock(update_mutex);
      update_memory_usage_light();
      update_cpu_utilization_light();
    }

    // 每次调用都更新吞吐量（计算简单）
    update_throughput();

    last_update = std::chrono::steady_clock::now();
  }

  /**
   * @brief 记录任务完成
   * @param task_time 任务执行时间(毫秒)
   */
  void record_task_completion(double task_time_ms)
  {
    total_completed_tasks++;
    completed_tasks_since_last++;

    // 更新平均任务时间(指数加权移动平均)
    double current_avg = average_task_time.load(std::memory_order_relaxed);
    double new_avg = 0.7 * current_avg + 0.3 * task_time_ms;
    average_task_time.store(new_avg, std::memory_order_relaxed);
  }

  /**
   * @brief 计算综合负载分数
   * @return 负载分数(0.0-1.0)
   */
  double calculate_load_score() const
  {
    auto cpu = cpu_utilization.load(std::memory_order_relaxed);
    auto memory = memory_usage.load(std::memory_order_relaxed);
    auto queue_factor = std::min(queue_length.load(std::memory_order_relaxed) / 100.0, 1.0);

    // 加权计算综合负载分数
    return 0.4 * cpu + 0.3 * memory + 0.3 * queue_factor;
  }

private:
  /**
   * @brief 轻量级内存使用率更新（减少系统调用）
   */
  void update_memory_usage_light()
  {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now - last_memory_measurement)
                               .count();

    // 限制内存更新频率（至少间隔500ms）
    if (time_since_last < 500)
    {
      return;
    }

    double usage = 0.0;

#ifdef _WIN32
    // 使用性能计数器获取内存使用（比GetProcessMemoryInfo更高效）
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof(pmc)))
    {
      MEMORYSTATUSEX memoryStatus;
      memoryStatus.dwLength = sizeof(memoryStatus);
      if (GlobalMemoryStatusEx(&memoryStatus))
      {
        usage = static_cast<double>(pmc.WorkingSetSize) / memoryStatus.ullTotalPhys;
      }
    }
#elif defined(__linux__)
    // 读取/proc/self/statm获取进程内存使用
    std::ifstream statm_file("/proc/self/statm");
    if (statm_file)
    {
      size_t size, resident, share;
      statm_file >> size >> resident >> share;
      statm_file.close();

      // 获取系统总内存
      std::ifstream meminfo_file("/proc/meminfo");
      if (meminfo_file)
      {
        std::string line;
        size_t total_memory = 0;
        while (std::getline(meminfo_file, line))
        {
          if (line.find("MemTotal:") == 0)
          {
            std::istringstream iss(line.substr(9));
            iss >> total_memory;  // 单位是KB
            total_memory *= 1024; // 转换为字节
            break;
          }
        }
        meminfo_file.close();

        if (total_memory > 0)
        {
          long page_size = sysconf(_SC_PAGESIZE);
          usage = static_cast<double>(resident * page_size) / total_memory;
        }
      }
    }
#elif defined(__APPLE__)
    // macOS实现需要使用task_info
    struct rusage usage_info;
    if (getrusage(RUSAGE_SELF, &usage_info) == 0)
    {
      // 获取系统总内存
      size_t total_memory = 0;
      int mib[2] = {CTL_HW, HW_MEMSIZE};
      size_t length = sizeof(total_memory);
      sysctl(mib, 2, &total_memory, &length, NULL, 0);

      if (total_memory > 0)
      {
        usage = static_cast<double>(usage_info.ru_maxrss * 1024) / total_memory;
      }
    }
#endif

    memory_usage.store(usage, std::memory_order_relaxed);
    last_memory_measurement = now;
  }

  /**
   * @brief 获取进程CPU时间（减少调用频率的版本）
   */
  std::chrono::steady_clock::time_point get_process_cpu_time_light()
  {
#ifdef _WIN32
    // 使用性能计数器替代GetProcessTimes（减少CPU0占用）
    static FILETIME last_idle_time, last_kernel_time, last_user_time;
    FILETIME idle_time, kernel_time, user_time;

    if (GetSystemTimes(&idle_time, &kernel_time, &user_time))
    {
      // 计算CPU使用率而不是直接返回时间
      // 这里简化处理，实际应用中可能需要更复杂的计算
      last_idle_time = idle_time;
      last_kernel_time = kernel_time;
      last_user_time = user_time;
    }

    // 返回当前时间而不是CPU时间（因为我们使用性能计数器）
    return std::chrono::steady_clock::now();
#elif defined(__linux__) || defined(__APPLE__)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0)
    {
      // 用户时间 + 系统时间，转换为纳秒
      uint64_t total_time =
          (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000000LL +
          (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) * 1000LL;
      return std::chrono::steady_clock::time_point(
          std::chrono::nanoseconds(total_time));
    }
#endif

    return std::chrono::steady_clock::now();
  }

  /**
   * @brief 轻量级CPU利用率更新（减少系统调用）
   */
  void update_cpu_utilization_light()
  {
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
                               now - last_cpu_measurement)
                               .count();

    // 限制CPU更新频率（至少间隔200ms）
    if (time_since_last < 200)
    {
      return;
    }

    double utilization = 0.0;

#ifdef _WIN32
    // 使用性能计数器获取CPU使用率（减少CPU0占用）
    PDH_FMT_COUNTERVALUE value;
    if (PdhCollectQueryData(cpu_query) == ERROR_SUCCESS &&
        PdhGetFormattedCounterValue(cpu_counter, PDH_FMT_DOUBLE, NULL, &value) == ERROR_SUCCESS)
    {
      utilization = value.doubleValue / 100.0; // 转换为0.0-1.0范围
    }
#else
    // Linux和macOS实现保持不变
    auto current_cpu_time = get_process_cpu_time_light();

    // 计算时间差
    auto real_time_delta = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               now - last_cpu_measurement)
                               .count();
    auto cpu_time_delta = std::chrono::duration_cast<std::chrono::nanoseconds>(
                              current_cpu_time - prev_cpu_time)
                              .count();

    if (real_time_delta > 0)
    {
      utilization = static_cast<double>(cpu_time_delta) / real_time_delta;

// 考虑CPU核心数
#ifdef _WIN32
      SYSTEM_INFO sysInfo;
      GetSystemInfo(&sysInfo);
      utilization /= sysInfo.dwNumberOfProcessors;
#else
      utilization /= sysconf(_SC_NPROCESSORS_ONLN);
#endif
    }

    // 更新状态
    prev_cpu_time = current_cpu_time;
#endif

    cpu_utilization.store(utilization, std::memory_order_relaxed);
    last_cpu_measurement = now;
  }

  /**
   * @brief 更新吞吐量
   */
  void update_throughput()
  {
    auto now = std::chrono::steady_clock::now();
    auto time_delta = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now - last_throughput_measurement)
                          .count();

    if (time_delta > 0)
    {
      double current_throughput = static_cast<double>(completed_tasks_since_last) /
                                  (time_delta / 1000.0); // 转换为秒

      throughput.store(current_throughput, std::memory_order_relaxed);
      completed_tasks_since_last = 0;
      last_throughput_measurement = now;
    }
  }

  // 保留原始get_process_cpu_time用于需要精确CPU时间的场景
  std::chrono::steady_clock::time_point get_process_cpu_time()
  {
#ifdef _WIN32
    FILETIME createTime, exitTime, kernelTime, userTime;
    if (GetProcessTimes(GetCurrentProcess(), &createTime, &exitTime, &kernelTime, &userTime))
    {
      ULARGE_INTEGER user, kernel;
      user.LowPart = userTime.dwLowDateTime;
      user.HighPart = userTime.dwHighDateTime;
      kernel.LowPart = kernelTime.dwLowDateTime;
      kernel.HighPart = kernelTime.dwHighDateTime;

      // 转换为纳秒
      uint64_t total_time = (user.QuadPart + kernel.QuadPart) * 100;
      return std::chrono::steady_clock::time_point(
          std::chrono::nanoseconds(total_time));
    }
#elif defined(__linux__) || defined(__APPLE__)
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0)
    {
      // 用户时间 + 系统时间，转换为纳秒
      uint64_t total_time =
          (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000000000LL +
          (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) * 1000LL;
      return std::chrono::steady_clock::time_point(
          std::chrono::nanoseconds(total_time));
    }
#endif

    return std::chrono::steady_clock::now();
  }
};

int main()
{
  load_metrics metrics;

  // 模拟任务处理循环
  for (int i = 0; i < 100; ++i)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 更新队列长度和活跃线程数(示例值)
    metrics.queue_length.store(5 + i % 10, std::memory_order_relaxed);
    metrics.active_threads.store(4, std::memory_order_relaxed);

    // 记录任务完成
    metrics.record_task_completion(95 + i % 5);

    // 更新所有指标（轻量级版本）
    metrics.update_metrics_light();

    // 每10次输出一次指标
    if (i % 10 == 0)
    {
      std::cout << "CPU: " << metrics.cpu_utilization.load() * 100 << "%"
                << ", Memory: " << metrics.memory_usage.load() * 100 << "%"
                << ", Throughput: " << metrics.throughput.load() << " tasks/s"
                << ", Load Score: " << metrics.calculate_load_score() * 100 << "%"
                << std::endl;
    }
  }

  return 0;
}