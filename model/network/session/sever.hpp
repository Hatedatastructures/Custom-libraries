#include "fundamental.hpp"

using namespace conversation::fundamental;

int main()
{
  boost::asio::io_context io;
  boost::asio::ip::tcp::acceptor acceptor(io, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), std::uint16_t(6779)));
  acceptor.listen();

  return 0;
}