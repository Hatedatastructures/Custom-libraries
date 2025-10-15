#include "fundamental.hpp"
#include <iostream>
#include <boost/asio.hpp>
#include <thread>

int main()
{
  using namespace conversation::fundamental;
  boost::asio::io_context io;
  boost::asio::ip::tcp::socket socket(io);
  socket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("124.71.136.228"), std::uint16_t(6779)));
  // boost::asio::ip::tcp::endpoint endpoint = socket.remote_endpoint();
  if(socket.is_open())
  {
    std::cout << "连接成功" << std::endl;
  }
  auto sess = std::make_shared<session<>>(std::move(socket));
  auto function = [](std::shared_ptr<session<>> , std::string_view data)
  {
    std::cout << "[server response] " << data << std::endl;
  };
  sess->set_reception_processing(function);
  sess->start();
  // 连接已建立，先发送一条示例消息
  sess->async_send_bytes({"hello, server"});
  std::jthread thread([&io]() {io.run();});
  std::this_thread::sleep_for(std::chrono::seconds(1));
  while (true)
  {
    std::string message;
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::cout << "请输入消息：->";
    std::getline(std::cin, message);
    sess->send_bytes(message);
  }
  
  thread.join();
  return 0;
}