#include <boost/pool/object_pool.hpp>
#include "../agreement/conversion.hpp"
#include "../session/fundamental.hpp"
#include <string>
#include <memory>
#include <iostream>


int main()
{
  // boost::object_pool<protocol::json> pool;
  // auto remover = [&pool](protocol::json* ptr)
  // {
  //   pool.destroy(ptr);
  // };
  // boost::pool<> pool2;
  // pool2.malloc_n(10);
  // auto v1 = std::shared_ptr<protocol::json>(pool.construct(), remover);
  // v1->set<std::string>({"key"}, {"value"});
  // std::cout << v1->get<std::string>({"key"}) << std::endl;
  boost::asio::io_context io_context;
  conversation::fundamental::session session(io_context);
  session.start();
  return 0;
}