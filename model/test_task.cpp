// #include "Task.hpp"
// #include <iostream>
// #include <chrono>
// #include <thread>
// #include <vector>
// #include <memory>
// #include <string>
// #include <cassert>

// using namespace task_structure;

// // 测试资源类
// class TestResource {
// public:
//     int value;
//     explicit TestResource(int v) : value(v) {
//         std::cout << "资源创建: " << value << std::endl;
//     }
//     ~TestResource() {
//         std::cout << "资源销毁: " << value << std::endl;
//     }
// };

// // 测试普通任务
// void test_normal_task() {
//     std::cout << "\n=== 测试普通任务 ===" << std::endl;
    
//     auto task = make_normal_task([]() {
//         std::cout << "普通任务执行" << std::endl;
//         std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     }, "普通任务", task_priority::normal);
    
//     assert(task->get_state() == task_state::pending);
//     task->execute();
//     assert(task->get_state() == task_state::completed);
//     std::cout << "普通任务测试通过" << std::endl;
// }

// // 测试带结果的任务
// void test_result_task() {
//     std::cout << "\n=== 测试带结果任务 ===" << std::endl;
    
//     auto task = make_result_task([]() -> int {
//         std::cout << "计算任务执行" << std::endl;
//         return 42;
//     }, "计算任务", task_priority::high);
    
//     assert(task->get_state() == task_state::pending);
//     task->execute();
//     assert(task->get_state() == task_state::completed);
    
//     auto result = task->get_result();
//     assert(result == 42);
//     std::cout << "结果任务测试通过，结果: " << result << std::endl;
// }

// // 测试优先级任务
// void test_priority_task() {
//     std::cout << "\n=== 测试优先级任务 ===" << std::endl;
    
//     auto high_task = make_priority_task<int>([]() -> int {
//         std::cout << "高优先级任务执行" << std::endl;
//         return 100;
// }, task_priority::high, "高优先级任务");
    
//     auto low_task = make_priority_task<int>([]() -> int {
//         std::cout << "低优先级任务执行" << std::endl;
//         return 10;
//     }, task_priority::low, "低优先级任务");
    
//     // assert(high_task->get_priority() == task_priority::high);
//     // assert(low_task->get_priority() == task_priority::low);
    
//     high_task->execute();
//     low_task->execute();
    
//     assert(high_task->get_result() == 100);
//     assert(low_task->get_result() == 10);
//     std::cout << "优先级任务测试通过" << std::endl;
// }

// // 测试超时任务
// void test_timeout_task() 
// {
//     std::cout << "\n=== 测试超时任务 ===" << std::endl;
    
//     // 正常完成的任务
//     auto normal_task = make_timeout_task<int>([]() -> int {
//         std::this_thread::sleep_for(std::chrono::milliseconds(50));
//         return 123;
//     }, std::chrono::milliseconds(200), "正常超时任务");
    
//     normal_task->execute();
//     assert(normal_task->get_state() == task_state::completed);
//     assert(normal_task->get_result() == 123);
    
//     // 超时的任务
//     auto timeout_task = make_timeout_task<void>([]() {
//         std::this_thread::sleep_for(std::chrono::milliseconds(300));
//     }, std::chrono::milliseconds(100), "超时任务");
    
//     timeout_task->execute();
//     // assert(timeout_task->get_state() == task_state::timeout);
    
//     std::cout << "超时任务测试通过" << std::endl;
// }

// // 测试依赖任务
// void test_dependency_task() {
//     std::cout << "\n=== 测试依赖任务 ===" << std::endl;
    
//     // 创建基础任务
//     auto base_task1 = make_result_task([]() -> int {
//         std::cout << "基础任务1执行" << std::endl;
//         std::this_thread::sleep_for(std::chrono::milliseconds(50));
//         return 10;
//     }, "基础任务1");
    
//     auto base_task2 = make_result_task([]() -> int {
//         std::cout << "基础任务2执行" << std::endl;
//         std::this_thread::sleep_for(std::chrono::milliseconds(30));
//         return 20;
//     }, "基础任务2");
    
//     // 创建依赖任务
//     std::vector<std::shared_ptr<task_base>> dependencies = {base_task1, base_task2};
//     auto dep_task = make_dependency_task<int>([&]() -> int {
//         std::cout << "依赖任务执行" << std::endl;
//         return base_task1->get_result() + base_task2->get_result();
//     }, dependencies, "依赖任务");
    
//     // 检查依赖状态
//     assert(!dep_task->are_dependencies_satisfied());
    
//     // 执行基础任务
//     base_task1->execute();
//     assert(!dep_task->are_dependencies_satisfied());
    
//     base_task2->execute();
//     assert(dep_task->are_dependencies_satisfied());
    
//     // 执行依赖任务
//     dep_task->execute();
//     assert(dep_task->get_result() == 30);
    
//     std::cout << "依赖任务测试通过，结果: " << dep_task->get_result() << std::endl;
// }

// // 测试资源任务
// // void test_resource_task() {
// //     std::cout << "\n=== 测试资源任务 ===" << std::endl;
    
// //     // 使用现有资源
// //     auto resource = std::make_shared<TestResource>(42);
// //     auto resource_task = std::make_shared<task_reso<int, TestResource>>(
// //         [](std::shared_ptr<TestResource> res) -> int {
// //             std::cout << "资源任务执行，资源值: " << res->value << std::endl;
// //             return res->value * 2;
// //         }, resource, "资源任务");
    
// //     assert(!resource_task->is_resource_acquired());
// //     resource_task->execute();
// //     // assert(resource_task->get_result() == 84);
    
// //     // 使用资源工厂
// //     auto factory_task = std::make_shared<task_reso<int, TestResource>>(
// //         [](std::shared_ptr<TestResource> res) -> int {
// //             std::cout << "工厂资源任务执行，资源值: " << res->value << std::endl;
// //             return res->value + 100;
// //         }, 
// //         []() -> std::shared_ptr<TestResource> {
// //             std::cout << "创建工厂资源" << std::endl;
// //             return std::make_shared<TestResource>(99);
// //         },
// //         [](std::shared_ptr<TestResource> res) {
// //             std::cout << "清理工厂资源: " << res->value << std::endl;
// //         },
// //         "工厂资源任务");
    
// //     factory_task->execute();
// //     // assert(factory_task->get_result() == 199);
    
// //     std::cout << "资源任务测试通过" << std::endl;
// // }

// // 测试性能优化
// void test_performance_optimizations() {
//     std::cout << "\n=== 测试性能优化 ===" << std::endl;
    
//     // 测试依赖缓存机制
//     auto base_task = make_normal_task([]() {
//         std::this_thread::sleep_for(std::chrono::milliseconds(10));
//     }, "基础任务");
    
//     auto dep_task = make_dependency_task<void>([]() {
//         std::cout << "依赖任务执行" << std::endl;
//     }, {base_task}, "缓存测试任务");
    
//     // 多次检查依赖状态（测试缓存）
//     auto start = std::chrono::high_resolution_clock::now();
//     for (int i = 0; i < 1000; ++i) {
//         dep_task->are_dependencies_satisfied();
//     }
//     auto end = std::chrono::high_resolution_clock::now();
//     auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
//     std::cout << "1000次依赖检查耗时: " << duration.count() << " 微秒" << std::endl;
    
//     // 执行基础任务后再次测试
//     base_task->execute();
    
//     start = std::chrono::high_resolution_clock::now();
//     for (int i = 0; i < 1000; ++i) {
//         dep_task->are_dependencies_satisfied();
//     }
//     end = std::chrono::high_resolution_clock::now();
//     duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
//     std::cout << "基础任务完成后1000次依赖检查耗时: " << duration.count() << " 微秒" << std::endl;
//     std::cout << "性能优化测试通过" << std::endl;
// }

// // 测试异常处理
// // void test_exception_handling() {
// //     std::cout << "\n=== 测试异常处理 ===" << std::endl;
    
// //         auto exception_task = make_result_task<void>([]() -> void {
// //         throw std::runtime_error("测试异常");
// //     }, "异常任务");

    
// //     try {
// //         exception_task->execute();
// //         assert(false); // 不应该到达这里
// //     } catch (const std::exception& e) {
// //         std::cout << "捕获异常: " << e.what() << std::endl;
// //         assert(exception_task->get_state() == task_state::failed);
// //     }
    
// //     std::cout << "异常处理测试通过" << std::endl;
// // }

// // 测试并发安全性
// // void test_thread_safety() {
// //     std::cout << "\n=== 测试线程安全性 ===" << std::endl;
    
// //     auto shared_resource = std::make_shared<TestResource>(0);
// //     std::vector<std::thread> threads;
// //     std::atomic<int> counter{0};
    
// //     // 创建多个线程同时访问资源任务
// //     for (int i = 0; i < 10; ++i) {
// //         threads.emplace_back([&, i]() {
// //             auto task = std::make_shared<task_reso<void, TestResource>>(
// //                 [&](std::shared_ptr<TestResource> res) {
// //                     res->value += i;
// //                     counter.fetch_add(1);
// //                     std::this_thread::sleep_for(std::chrono::milliseconds(1));
// //                 }, shared_resource, "并发任务" + std::to_string(i));
            
// //             task->execute();
// //         });
// //     }
    
// //     // 等待所有线程完成
// //     for (auto& t : threads) {
// //         t.join();
// //     }
    
// //     assert(counter.load() == 10);
// //     std::cout << "最终资源值: " << shared_resource->value << std::endl;
// //     std::cout << "线程安全性测试通过" << std::endl;
// // }

// int main() {
//     std::cout << "开始Task.hpp优化版本测试" << std::endl;
    
//     try {
//         test_normal_task();
//         test_result_task();
//         test_priority_task();
//         test_timeout_task();
//         test_dependency_task();
//         // test_resource_task();
//         test_performance_optimizations();
//         // test_exception_handling();
//         // test_thread_safety();
        
//         std::cout << "\n=== 所有测试通过! ===" << std::endl;
//     } catch (const std::exception& e) {
//         std::cerr << "测试失败: " << e.what() << std::endl;
//         return 1;
//     }
    
//     return 0;
// }
// // #include"Task.hpp"
// // #include <iostream>
// // int main()
// // {
// //         // 创建普通任务
// //     auto norm_task = task_structure::make_normal_task(
// //       [](){ std::cout << "Normal task\n"; }, 
// //       "normal_task"
// //     );
// //     norm_task->execute();
// //     std::cout << "Normal task executed\n";
// //     // 创建带返回值的任务
// //     auto rslt_task = task_structure::make_result_task(
// //       [](){ return 42; }, 
// //       "result_task"
// //     );
// //     rslt_task->execute();
// //     // 创建优先级任务
// //     auto prio_task = task_structure::make_priority_task<int>(
// //       [](){ return 100; }, 
// //       task_structure::task_priority::high, 
// //       "prio_task"
// //     );
// //     std::cout << "Priority task created\n";
// //     prio_task->execute();
// //     std::cout <<  prio_task->get_future().get() << std::endl;
// //     // 创建超时任务（5秒超时）
// //     auto time_task = task_structure::make_timeout_task(
// //       [](){ /* 可能超时的操作 */ }, 
// //       std::chrono::seconds(5), 
// //       "timeout_task"
// //     );
// //     time_task->execute();
// //     // 等待任务完成并获取结果
// //     rslt_task->wait();
// //     int result = rslt_task->get_result();
// //     std::cout << result << std::endl; // 输出: 42
// //     return 0;
// // }
// #include "Task_queue.hpp"
#include "Task.hpp"
#include <vector>
#include <iostream>
#include <string>
int main()
{
    std::vector<std::shared_ptr<task_structure::task_base>> tasks;
    auto task1 = task_structure::make_priority_task<std::string>
    ([](){std::cout << "Task 1\n" << std::endl;return std::string("Hello"); },task_structure::task_priority::high, "task1");
    tasks.push_back(task1);
    tasks[0]->execute();
    auto task2 = task_structure::make_normal_task([](){std::cout << "Task 2\n" << std::endl; }, "task2");
    tasks.push_back(task2);
    tasks[1]->execute();
    std::cout << tasks[0]->get_task_id() << std::endl;
    std::cout << tasks[1]->get_task_id() << std::endl;
    std::cout << tasks[0]->get_result()  << std::endl;
    return 0;
}