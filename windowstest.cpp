#include <iostream>
#include <boost/version.hpp>
#include <boost/unordered/concurrent_flat_map.hpp>
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/pool/pool.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <memory>
using namespace std;
using namespace boost;
int main()
{
  std::shared_ptr<int> p(new int(1));
  boost::unordered::unordered_flat_map<std::string, int> map;
  map["hello"] = 1;
  map["world"] = 2;
  // std::cout << map["hello"] << std::endl;
  std::cout << map["world"] << std::endl;
  boost::pool pool(sizeof(int));
  std::cout << "Boost version: " << BOOST_LIB_VERSION << std::endl;
  return 0;
}