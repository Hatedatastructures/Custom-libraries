// 服务器
#pragma once
#include <memory>
#include <boost/asio.hpp>
#include "./module/Thread_pool.hpp"
#include "agreement.hpp"

namespace framework
{
  class stream_gateway
  {
  private:
    wan::thread_pool thread_pool;
    boost::asio::io_context io_context;
  };
  class datagram_gateway
  {

  };
  class application_host
  {
  private:
    stream_gateway stream_gateway;
  };
}
