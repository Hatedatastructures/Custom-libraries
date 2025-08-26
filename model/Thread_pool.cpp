#include <iostream>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <iomanip>
#include <memory>
#include <numeric>
#include <algorithm>
#include "Task.hpp"
#include "Cohort.hpp"
using namespace _implemented_internally::structure_cohort;
using namespace _implemented_internally::structure_task;

// 命名空间补充（与任务系统保持一致）
namespace task_system {
    // 实际调度器应基于线程池实现，此处简化为模拟调度
    class Scheduler {
    private:
        // 模拟线程池执行任务（实际应使用多线程）
        static void execute_task(std::shared_ptr<task_base> task) {
            if (!task) return;

            // 处理依赖任务：先执行所有依赖
            if (auto dep_task = dynamic_cast<task_depn<int>*>(task.get())) {
                auto deps = dep_task->get_pending_dependencies();
                for (auto& dep : deps) {
                    execute_task(dep); // 递归执行依赖
                }
                dep_task->wait_for_dependencies(std::chrono::seconds(5)); // 等待依赖完成
            }

            // 标记任务运行并执行
            if (task->mark_running()) {
                try {
                    task->execute();
                } catch (const task_anomaly& e) {
                    std::cerr << "任务执行异常 (ID: " << e.get_task_id() << "): " << e.what() << std::endl;
                    task->mark_failed();
                }
            }

            // 处理协程任务：恢复执行
            if (auto coro_task = dynamic_cast<task_coro<int>*>(task.get())) {
                while (!coro_task->is_coroutine_done()) {
                    coro_task->resume_coroutine(); // 主动恢复协程
                    std::this_thread::yield();
                }
            }
        }

    public:
        // 提交任务并等待完成（使用任务内置的wait方法，避免轮询）
        template<typename TaskType>
        static void submit_and_wait(std::vector<std::shared_ptr<TaskType>>& tasks) {
            // 1. 提交所有任务到调度器（模拟线程池分配）
            for (auto& task : tasks) {
                std::thread(execute_task, task).detach(); // 实际应使用线程池，避免创建大量线程
            }

            // 2. 等待所有任务完成（使用任务的wait方法，基于条件变量）
            for (auto& task : tasks) {
                if (!task->wait_for(std::chrono::seconds(10))) { // 10秒超时保护
                    std::cerr << "任务超时未完成: " << task->get_task_name() << std::endl;
                    task->mark_timeout();
                }
            }
        }
    };

    // 性能测试工具类（保持不变）
    class PerformanceTester {
    public:
        using TimePoint = std::chrono::steady_clock::time_point;
        using Duration = std::chrono::duration<double, std::milli>;

        static TimePoint start() { return std::chrono::steady_clock::now(); }
        static double end(const TimePoint& start) {
            return Duration(std::chrono::steady_clock::now() - start).count();
        }
        static void print_result(const std::string& test_name, size_t task_count, double total_time) {
            const double throughput = task_count / (total_time / 1000.0);
            const double avg_latency = total_time / task_count;
            std::cout << "=== " << test_name << " 测试结果 ===" << std::endl;
            std::cout << "任务总数: " << task_count << std::endl;
            std::cout << "总耗时: " << std::fixed << std::setprecision(2) << total_time << " ms" << std::endl;
            std::cout << "平均延迟: " << std::fixed << std::setprecision(4) << avg_latency << " ms" << std::endl;
            std::cout << "吞吐量: " << std::fixed << std::setprecision(2) << throughput << " 任务/秒" << std::endl;
            std::cout << "========================================\n" << std::endl;
        }
    };

    // 1. 普通任务测试（保持不变）
    void test_normal_tasks(size_t task_count) {
        std::vector<std::shared_ptr<task_norm>> tasks;
        tasks.reserve(task_count);
        for (size_t i = 0; i < task_count; ++i) {
            tasks.emplace_back(std::make_shared<task_norm>(
                []() { std::this_thread::sleep_for(std::chrono::nanoseconds(100)); },
                "normal_task_" + std::to_string(i)
            ));
        }
        auto start = PerformanceTester::start();
        Scheduler::submit_and_wait(tasks);
        PerformanceTester::print_result("普通任务", task_count, PerformanceTester::end(start));
    }

    // 2. 带返回值任务测试（保持不变）
    void test_result_tasks(size_t task_count) {
        std::vector<std::shared_ptr<task_rslt<int>>> tasks;
        tasks.reserve(task_count);
        for (size_t i = 0; i < task_count; ++i) {
            tasks.emplace_back(std::make_shared<task_rslt<int>>(
                []() -> int { 
                    std::this_thread::sleep_for(std::chrono::nanoseconds(100));
                    return 42; 
                },
                "result_task_" + std::to_string(i)
            ));
        }
        auto start = PerformanceTester::start();
        Scheduler::submit_and_wait(tasks);
        PerformanceTester::print_result("带返回值任务", task_count, PerformanceTester::end(start));
    }

    // 3. 依赖任务测试（修复依赖执行逻辑）
    void test_dependency_tasks(size_t chain_length) {
        std::vector<std::shared_ptr<task_depn<int>>> tasks;
        std::shared_ptr<task_base> prev_task = std::make_shared<task_norm>([]() {}, "dep_root");

        for (size_t i = 0; i < chain_length; ++i) {
            auto task = std::make_shared<task_depn<int>>(
                []() -> int { 
                    std::this_thread::sleep_for(std::chrono::nanoseconds(100));
                    return 0; 
                },
                prev_task, // 依赖前一个任务
                "dep_task_" + std::to_string(i)
            );
            tasks.push_back(task);
            prev_task = task; // 更新前序任务
        }

        auto start = PerformanceTester::start();
        Scheduler::submit_and_wait(tasks);
        PerformanceTester::print_result("依赖链任务 (" + std::to_string(chain_length) + "个节点)",
                                       chain_length, PerformanceTester::end(start));
    }

    // 4. 超时任务测试（增加超时回调验证）
    void test_timeout_tasks(size_t task_count, bool should_timeout) {
        std::vector<std::shared_ptr<task_time<int>>> tasks;
        tasks.reserve(task_count);

        auto task_duration = should_timeout ? 
            std::chrono::milliseconds(20) : 
            std::chrono::milliseconds(5);
        auto timeout = std::chrono::milliseconds(10);

        for (size_t i = 0; i < task_count; ++i) {
            tasks.emplace_back(std::make_shared<task_time<int>>(
                [task_duration]() -> int { 
                    std::this_thread::sleep_for(task_duration);
                    return 0; 
                },
                timeout,
                []() { std::cout << "任务超时回调触发" << std::endl; }, // 超时回调
                "timeout_task_" + std::to_string(i)
            ));
        }

        auto start = PerformanceTester::start();
        Scheduler::submit_and_wait(tasks);
        std::string test_name = should_timeout ? "超时任务（触发超时）" : "超时任务（正常完成）";
        PerformanceTester::print_result(test_name, task_count, PerformanceTester::end(start));
    }

    // 5. 协程任务测试（修复协程恢复逻辑）
    #if __cpp_lib_coroutines >= 201902L
    task_coro<int> create_coro_task(int value) {
        co_await std::suspend_always{}; // 模拟挂起，需要resume才能继续
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
        co_return value;
    }

    void test_coroutine_tasks(size_t task_count) {
        std::vector<std::shared_ptr<task_coro<int>>> tasks;
        tasks.reserve(task_count);

        for (size_t i = 0; i < task_count; ++i) {
            auto coro = create_coro_task(i); // 创建协程
            auto handle = std::coroutine_handle<task_coro<int>::promise_type>::from_promise(
                coro.promise()
            );
            tasks.emplace_back(std::make_shared<task_coro<int>>(
                handle,
                "coro_task_" + std::to_string(i)
            ));
        }

        auto start = PerformanceTester::start();
        Scheduler::submit_and_wait(tasks);
        PerformanceTester::print_result("协程任务", task_count, PerformanceTester::end(start));
    }
    #endif

    // 6. 优先级任务测试（保持不变）
    void test_priority_scheduling(size_t task_count) {
        std::vector<std::shared_ptr<task_norm>> tasks;
        tasks.reserve(task_count);

        std::vector<urgency_level> priorities = {
            urgency_level::lowest, urgency_level::low, urgency_level::normal,
            urgency_level::high, urgency_level::highest
        };

        for (size_t i = 0; i < task_count; ++i) {
            auto prio = priorities[i % priorities.size()];
            tasks.emplace_back(std::make_shared<task_norm>(
                []() { std::this_thread::sleep_for(std::chrono::nanoseconds(100)); },
                "prio_task_" + std::to_string(i),
                prio
            ));
        }

        auto start = PerformanceTester::start();
        Scheduler::submit_and_wait(tasks);
        PerformanceTester::print_result("多优先级任务调度", task_count, PerformanceTester::end(start));
    }
} // namespace task_system

int main() {
    const size_t BASE_TASK_COUNT = 1000; // 减少任务数量，避免线程创建过多

    using namespace task_system;
    test_normal_tasks(BASE_TASK_COUNT);
    test_result_tasks(BASE_TASK_COUNT);
    test_dependency_tasks(BASE_TASK_COUNT / 10);
    test_timeout_tasks(BASE_TASK_COUNT / 2, false);
    test_timeout_tasks(BASE_TASK_COUNT / 2, true);
    test_priority_scheduling(BASE_TASK_COUNT);

    #if __cpp_lib_coroutines >= 201902L
    test_coroutine_tasks(BASE_TASK_COUNT);
    #else
    std::cout << "=== 协程任务测试 ===" << std::endl;
    std::cout << "编译器不支持C++20协程，跳过测试" << std::endl;
    std::cout << "========================================\n" << std::endl;
    #endif

    return 0;
}