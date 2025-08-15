#include <iostream>
#include "Syncs.hpp"
#include "Concurrent_container.hpp"

// void read(con::mpmc_priority_queue<int>& q)
// {
//   while (true)
//   {
//     int val ;
//     q.pop(val);
//     std::cout << "read: " << val << std::endl;
//   }
// }
int main()  
{
  // con::mpmc_priority_queue<int> q(10);
  // std::thread read_thread([&q](){read(q);});
  // uint64_t i = 0;
  // while(true)
  // {
  //   q.push(i++);
  // }
  // read_thread.join();
  std::string s = "hello world";
  std::string t = "hello";
  con::concurrent_map<std::string,std::string> map;
  con::concurrent_set<std::string> set;
  con::concurrent_unordered_map<std::string,std::string> unordered_map;
  con::concurrent_unordered_set<std::string> unordered_set;
  set.insert(s);
  unordered_map.insert({s,t});
  map.insert({s,t});
  map.emplace("hello","world");
  auto snap = map.snapshot();   // 线程安全快照
  for(auto key_value : snap)
  {
    std::cout << key_value.first << " " << key_value.second << std::endl;
  }
  return 0;
}