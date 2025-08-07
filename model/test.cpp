// #include "produce_consume.hpp"
// #include <ctime>
// #include <thread>
// #include <string>
// #include <windows.h>
#include <iostream>
#include <future>
#include <windows.h>
#include "boost/asio.hpp"
#include "Thread_pool.hpp"
// void read(producer_consumer_queues<std::string> &q)
// {
//   for(int i = 0; i < 100; i++)
//   {
//     std::string ttmp;
//     if(q.pop(ttmp))
//     {
//       std::cout << ttmp << std::endl;
//     }
//     else
//     {
//       std::cout << "队列为空" << std::endl;
//     }
//     // Sleep(100);
//   }
// }
// void write(producer_consumer_queues<std::string> &q)
// {
//   for(int i = 0; i < 100; i++)
//   {
//     q.push("单生产单消费队列测试： 这是第" + std::to_string(i) + "个数据");
//     // Sleep(100);
//   }
//   // std::cout << "write " << j << std::endl;
// }
// // void test_queue()
// // {
// //   producer_consumer_queue<std::string> q;
// //   std::thread write_thread ([&q](){write(q);});
// //   std::thread read_thread ([&q](){read(q);});
// //   write_thread.join();
// //   read_thread.join();
// // }
// void tests_queue()
// {
//   producer_consumer_queues<std::string> q;
//   std::thread write_thread ([&q](){write(q);});
//   std::thread read_thread ([&q](){read(q);});
//   Sleep(5000);
//   q.flush();
//   write_thread.join();
//   read_thread.join();
// }
// double temp_test(double first ,double second)
// {
//   return first + second;
// }
// std::atomic<uint64_t> num;
// void test()
// {
//   num++;
// }
void test()
{
  boost::asio::io_context io_context;
  boost::asio::ip::tcp ip;
  boost::asio::ip::tcp::socket socket(io_context);
}
int main()
{
  // test_queue();
  // tests_queue();
  // std::cout << "hello world" << std::endl;
  // constexpr std::invoke_result_t<decltype(temp_test),double,double>
  // thread_pool threads(32ULL,16ULL,64ULL);
  // uint64_t arr;
  // {
  //   std::thread _threads ([&]
  //     {
  //       uint64_t  size = 10000000;
  //       while(size--)
  //       {
  //         threads.submit(test);
  //       }
  //     });
  //   Sleep(1000);
  //   std::cout << num << " ";
  //   _threads.join();
  // }
  // std::cout << num << std::endl;
  // arr = threads.active_threads();
  // std::cout << arr << std::endl;
  // threads.submit(test);
  // threads.submit(test);
  return  0;
}