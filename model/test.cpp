#include "Thread_pool.hpp"
#include <memory>
int main()
{
  auto thread_pool_test = con::make_lightweight_pool(32);
  auto value =  thread_pool_test->get_config();
  thread_pool_test->start();
  auto func = [](std::string s)
  {
    std::cout << s << std::endl;
    return std::string("执行完毕！");
  };
  auto return_value = thread_pool_test->submit(func, "hello world");
  std::cout << return_value.get() << std::endl;

  auto message = thread_pool_test->get_performance_report();
  std::cout << message << std::endl;
  return 0;
}