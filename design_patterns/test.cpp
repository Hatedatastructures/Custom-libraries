#include "produce_consume.hpp"
#include <ctime>
#include <thread>
#include <string>
#include <windows.h>
#include <iostream>
void read(producer_consumer_queues<std::string> &q)
{
  for(int i = 0; i < 100; i++)
  {
    std::string ttmp;
    if(q.pop(ttmp))
    {
      std::cout << ttmp << std::endl;
    }
    else
    {
      std::cout << "队列为空" << std::endl;
    }
    // Sleep(100);
  }
}
void write(producer_consumer_queues<std::string> &q)
{
  for(int i = 0; i < 100; i++)
  {
    q.push("单生产单消费队列测试： 这是第" + std::to_string(i) + "个数据");
    // Sleep(100);
  }
  // std::cout << "write " << j << std::endl;
}
// void test_queue()
// {
//   producer_consumer_queue<std::string> q;
//   std::thread write_thread ([&q](){write(q);});
//   std::thread read_thread ([&q](){read(q);});
//   write_thread.join();
//   read_thread.join();
// }
void tests_queue()
{
  producer_consumer_queues<std::string> q;
  std::thread write_thread ([&q](){write(q);});
  std::thread read_thread ([&q](){read(q);});
  Sleep(5000);
  q.flush();
  write_thread.join();
  read_thread.join();
}
int main()
{
  // test_queue();
  tests_queue();
  // std::cout << "hello world" << std::endl;
  return  0;
}