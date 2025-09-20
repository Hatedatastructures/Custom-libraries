#include "container/container.hpp"
#include "concurrent/container.hpp"
using namespace wan;
int main()
{
  wan::scl::string string_val("hello,world");
  std::cout << string_val << std::endl;
  mco::concurrent_map<mco::concurrent_string,mco::concurrent_string> map;
  map.insert({"hello","world"});
  std::cout << map.at(mco::concurrent_string("hello")).c_str() << std::endl;
  return 0;
}