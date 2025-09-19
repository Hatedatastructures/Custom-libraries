#include "container/container.hpp"
#include "concurrent/container.hpp"
using namespace wan;
int main()
{
  wan::scl::string string_val("hello,world");
  std::cout << string_val << std::endl;
  return 0;
}