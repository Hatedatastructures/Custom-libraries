#include <iostream>
#include <fstream>
#include <string>
#include <ctime>
#include <vector>
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>
#include <boost/circular_buffer.hpp>

using log_string = std::string;
class queue_buffer
{
  public:
    boost::circular_buffer<log_string> first;
    boost::circular_buffer<log_string> second;
    std::queue<log_string>* input;
    std::queue<log_string>* output;
    std::mutex mutex;
  public:
    queue_buffer()
    {
      first.resize(1000);
      second.resize(1000);
    }
    void push(log_string s)
    {
      first.push_back(s);
    }
};
int main()
{
  //std::cout << "Hello World!" << std::endl;
  queue_buffer buffer;
  buffer.push("Hello World!");
  std::cout << buffer.first.back() << std::endl;
  return 0;
}