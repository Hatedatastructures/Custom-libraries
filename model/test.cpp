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
#include "Uint.hpp"
#include <iostream>
int main()
{
  {
    internals::structure_t::uint_standard d([](){ return 42; });
    d.execute();
    // int a = d.get_result();
    // std::cout << a << std::endl;
    std::cout << d.get_future().get() << std::endl;
  }
  {
    internals::structure_t::uint_priority p([](){ return 100; }, "High Priority Task", internals::structure_t::urgency_level::high);
    std::cout << p.get_priority() << std::endl;
    p.execute();
    std::cout << p.get_future().get() << std::endl;
    std::cout << "Task ID: " << p.get_identifier() << std::endl;
    std::cout << p.get_task_name() << std::endl;

    std::cout << "-----------------" << std::endl;

    internals::structure_t::uint_priority p2([](){ return 200; }, "Higher Priority Task", internals::structure_t::urgency_level::highest);

    std::cout << (p2 < p) << std::endl; // false
    std::cout << (p2 > p) << std::endl; // true
    p2.execute();
    std::cout << p2.get_future().get() << std::endl;
    std::cout << "Task ID: " << p2.get_identifier() << std::endl;
    std::cout << p2.get_task_name() << std::endl;

  }
  return 0;
}