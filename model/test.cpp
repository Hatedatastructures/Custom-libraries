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
#include "Uint.hpp"
#include <iostream>
using namespace internals::structure_t;
class internal_future 
{
public:
  template<class deduction_t>
  internal_future(std::future<deduction_t> f)
  : _ptr(std::make_shared<deduction_model<deduction_t>>(std::move(f))) {}

  void wait() const { _ptr->wait(); }
  bool valid() const { return _ptr->valid(); }
  template<class convert_t>
  convert_t get() const
  {
    auto derived_ptr = std::dynamic_pointer_cast<deduction_model<convert_t>>(_ptr);
    if (!derived_ptr) 
    {
      throw anomaly("类型转换失败: 内部类型不匹配", 0);
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
  template<class deduction_t>
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
int main()
{
  // {
  //   internals::structure_t::uint_ordinary d([](){ return 42; });
  //   d.execute();
  //   // int a = d.get_result();
  //   // std::cout << a << std::endl;
  //   std::cout << d.get_future().get() << std::endl;
  // }
  {
    internals::structure_t::uint_ordinary p([](){ std::cout << "hello,worrld!" << std::endl; },
     "High Priority Task", internals::structure_t::urgency_level::high);
    std::cout << p.get_priority() << std::endl;
    p.execute();
    // std::cout << p.get_future().get() << std::endl;
    std::cout << "Task ID: " << p.get_identifier() << std::endl;
    std::cout << p.get_task_name() << std::endl;

    std::cout << "-----------------" << std::endl;

    internals::structure_t::uint_ordinary p2([](){ std::cout << "stream!" << std::endl; }, 
    "Higher Priority Task", internals::structure_t::urgency_level::highest);

    std::cout << (p2 < p) << std::endl; // false
    std::cout << (p2 > p) << std::endl; // true
    p2.execute();
    // std::cout << p2.get_future().get() << std::endl;
    std::cout << "Task ID: " << p2.get_identifier() << std::endl;
    std::cout << p2.get_task_name() << std::endl;

  }
  {
    // 测试返回值自动推导
    internal_future fut1(std::async(std::launch::async, []() { return 42; }));
    auto result = fut1.get<int>();
    std::cout << "Result: " << result << std::endl; // 输出: Result: 42
  }
  return 0;
}