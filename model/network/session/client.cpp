// #include "fundamental.hpp"
// #include <iostream>
// #include <boost/asio.hpp>
// #include <thread>

// int main()
// {
//   using namespace conversation::fundamental;
//   boost::asio::io_context io;
//   boost::asio::ip::tcp::socket socket(io);
//   socket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("124.71.136.228"), std::uint16_t(6779)));
//   // boost::asio::ip::tcp::endpoint endpoint = socket.remote_endpoint();
//   if(socket.is_open())
//   {
//     std::cout << "连接成功" << std::endl;
//   }
//   auto sess = std::make_shared<session<>>(std::move(socket));
//   auto function = [](std::shared_ptr<session<>> , std::string_view data)
//   {
//     std::cout << "[server response] " << data << std::endl;
//   };
//   sess->set_reception_processing(function);
//   sess->start();
//   // 连接已建立，先发送一条示例消息
//   sess->async_send_bytes({"hello, server"});
//   std::jthread thread([&io]() {io.run();});
//   std::this_thread::sleep_for(std::chrono::seconds(1));
//   while (true)
//   {
//     std::string message;
//     std::this_thread::sleep_for(std::chrono::milliseconds(100));
//     std::cout << "请输入消息：->";
//     std::getline(std::cin, message);
//     sess->send_bytes(message);
//   }
  
//   thread.join();
//   return 0;
// }

#include "../session/fundamental.hpp"
#include "../session/conversation.hpp"
#include <boost/asio.hpp>
#include <iostream>
#include <memory>
#include <string>
#include <chrono>

int main()
{
  // using namespace conversation::fundamental;

  // boost::asio::io_context io;
  // auto sess = std::make_shared<session<>>(io, std::string{"124.71.136.228"}, std::uint16_t(6779));
  // auto function = [sess](std::shared_ptr<session<>> , std::string_view data)
  // {
  //   std::cout << "[receive] " << data << std::endl;
  // };
  // sess->set_reception_processing(function);
  // auto send_message = [sess](const boost::system::error_code& ec)
  // {
  //   if(ec)
  //   {
  //     std::cout << "[connect error] code=" << ec.value() << std::endl;
  //     return;
  //   }
  //   else
  //   {
  //     sess->async_send_bytes({"hello, server"});
  //     // sess->send_bytes({"hello, server! I am client."}); // 同步发送，阻塞当前线程
  //     std::this_thread::sleep_for(std::chrono::seconds(1));
  //     sess->close();
  //   }
  // };
  // sess->async_connect({"124.71.136.228"}, std::uint16_t(6779), send_message);
  // io.run();


  // using namespace conversation::fundamental;
  // boost::asio::io_context io;
  // auto sess = std::make_shared<session<request,response>>(io);
  // conversation::session_management sess_man(io);
  // sess_man.start();
  
  // boost::asio::ip::tcp::endpoint server_ip(boost::asio::ip::make_address("124.71.136.228"), std::uint16_t(6779));
  // auto client_sess = sess_man.create_client_session(server_ip);
  // if(client_sess.second != nullptr)
  // {
  //   std::cout << "[connect success] session_id=" << client_sess.first  << " " <<
  //   client_sess.second->get_session_id() << std::endl;
  // }
  using namespace conversation::fundamental;
  boost::asio::io_context io;
  std::shared_ptr<session<>> sess = std::make_shared<session<>>(io);
  auto value = sess->connect({"124.71.136.228"}, std::uint16_t(6779));
  if(!value)
    sess->send_bytes({"hello, server"});
  boost::asio::ip::tcp::socket sock(io);
  sock.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address("124.71.136.228"), std::uint16_t(6779)));
  return 0;
}