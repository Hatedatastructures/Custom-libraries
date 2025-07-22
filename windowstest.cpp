#include <iostream>
#include <string>
#include <boost/version.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/pool/pool.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/circular_buffer.hpp>
#include <memory>
using namespace std;
using namespace boost;
int main()
{
  boost::circular_buffer<std::string> cb(10);
  cb.push_back("hello");
  cb.push_back("world");
  auto it = cb.begin() + 1;
  std::cout << "大小 :" << cb.size() << std::endl;
  std::cout << "容量 :" << cb.capacity() << std::endl;
  std::cout << "最大容量:" << cb.max_size() << std::endl;
  std::cout << *it << std::endl;
  cout << cb[0] << endl;
  return 0;
}