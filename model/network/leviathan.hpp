// 服务器
#pragma once
#include <memory>
#include <boost/asio.hpp>
#include "./module/Thread_pool.hpp"
#include "agreement.hpp"

namespace framework
{
  class tcp_server
  {
  private:
    wan::thread_pool thread_pool;
    boost::asio::io_context io_context;
  };
  class udp_server
  {

  };
  class http_server
  {
  private:
    tcp_server tcp_server;
  };
}
