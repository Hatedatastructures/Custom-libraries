
// 演示：连接到公网服务器 124.71.136.228:6779，并发送/接收消息
#include "../session/fundamental.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>

int main()
{
  using namespace conversation::fundamental;

  // 1) 创建 IO 上下文与会话
  boost::asio::io_context io;
  auto sess = std::make_shared<session<>>(io, std::string{"124.71.136.228"}, static_cast<std::uint16_t>(6779));

  // 2) 设置接收处理：打印服务端返回的原始字节
  sess->set_reception_processing([](session<>::session_ptr /*self*/, std::string_view data) {
    std::cout << "[receive] " << data << std::endl;
  });

  // 3) 异步连接远端，成功后启动读取与心跳，并发送一条消息
  sess->async_connect("124.71.136.228", static_cast<std::uint16_t>(6779), [sess](const boost::system::error_code& ec) {
    if (ec)
    {
      std::cerr << "[connect error] " << ec.message() << std::endl;
      return;
    }
    std::cout << "[connected] " << sess->get_remote_address() << ":" << sess->get_remote_port() << std::endl;

    // 启动读取与心跳
    sess->start();

    // 发送原始字节（服务器协议未知，这里以简单字符串示例）
    sess->async_send_bytes("hello", [sess](const boost::system::error_code& send_ec) 
    {
      if (send_ec)
        std::cerr << "[send error] " << send_ec.message() << std::endl;
      else
        std::cout << "[send ok]" << std::endl;
    });
  });

  // 4) 运行事件循环；为了演示退出，这里设置 10s 定时关闭
  boost::asio::steady_timer t(io, std::chrono::seconds(10));
  t.async_wait([sess, &io](const boost::system::error_code&) {
    std::cout << "[demo end] closing session" << std::endl;
    sess->close();
    io.stop();
  });

  io.run();
  return 0;
}