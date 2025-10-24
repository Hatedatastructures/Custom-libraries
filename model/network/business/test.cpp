// #include "forwarder.hpp"

// int main()
// {
//   boost::asio::io_context io;
//   represents::transponder forwarder(io);
//   forwarder.add_information("example_upstream", "example.com", 80, false);
//   // forwarder.forward_sync();
  
//   return 0;
// }
#include "../session/fundamental.hpp"
#include "../session/conversation.hpp"
#include "forwarder.hpp"
#include <boost/asio/ts/buffer.hpp>
#include <boost/asio/ts/internet.hpp>
#include <iostream>
#include <format>
#include <memory>
#include <string>
#include <fstream>
#include <vector>

const std::string file_path = "webpage.html";

using namespace conversation::fundamental;

class server : public std::enable_shared_from_this<server>
{
private:
  // std::string _webpage;

  // std::string extract(std::string _file_path)
  // {
  //   std::ifstream file(_file_path);
  //   if(!file.is_open())
  //     return "";
  //   std::stringstream buffer;
  //   buffer << file.rdbuf();
  //   return buffer.str();
  // }
public:
  server(boost::asio::io_context &io_context, short port)
    : _io_context(io_context),
      _acceptor(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
      _manager(_io_context),_forwarder(_io_context)
  {}

  ~server()
  {
    _manager.stop();
  }

  void start()
  {
    _manager.start();
    _forwarder.add_information("www.baidu.com", "www.baidu.com", 80, false);
    // _webpage = extract(file_path);
    _do_accept();
  }

private:
  void _do_accept()
  {
    auto function = [this, self = shared_from_this()]
    (boost::system::error_code ec, boost::asio::ip::tcp::socket socket)
    {
      if(!ec)
      {
        boost::system::error_code ep_ec;
        auto ep = socket.remote_endpoint(ep_ec);
        if(!ep_ec)
          std::cout << std::format("[{:%Y-%m-%d %H:%M:%S}] [conversationss : {}] Accepted connection from: {}:{}\n",
            std::chrono::system_clock::now(),_manager.get_session_count(), ep.address().to_string(), ep.port());
        else
          std::cout << std::format("[{:%Y-%m-%d %H:%M:%S}] [conversationss : {}] Accepted connection\n",
            std::chrono::system_clock::now(), _manager.get_session_count());

        auto pair = _manager.create_server_session(std::move(socket));
        auto new_session = pair.second;
        if(new_session)
        {
          auto auto_reception_processing = [this](std::shared_ptr<session<>> sess, std::string_view data)
          { 
            // const std::string& body = _webpage;
            // std::string http_response =
            // "HTTP/1.1 200 OK\r\n"
            // "Content-Type: text/html; charset=utf-8\r\n"
            // "Content-Length: " + std::to_string(body.size()) + "\r\n"
            // "Connection: close\r\n"
            // "\r\n" + body;
            protocol::http::request<> http_request;
            http_request.from_string(data);
            auto req = _forwarder.forward_sync(http_request);
            sess->async_send_bytes(req.to_string(), nullptr); 
          };
          new_session->set_reception_processing(auto_reception_processing);
          new_session->start();
        }
      }
      else
      { 
        std::cerr << "Accept error: " << ec.message() << std::endl;
      }
      _do_accept();
    };
    _acceptor.async_accept(std::move(function));
  }
  boost::asio::io_context &_io_context;
  boost::asio::ip::tcp::acceptor _acceptor;
  conversation::session_management<> _manager;
  represents::transponder<> _forwarder;
};

int main()
{
  try
  {
    boost::asio::io_context io;
    auto srv = std::make_shared<server>(io, 6779);
    srv->start();
    io.run();
  }
  catch (std::exception &e)
  {
    std::cerr << "Exception: " << e.what() << std::endl;
  }
  return 0;
}