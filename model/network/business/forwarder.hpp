#include "../agreement/http.hpp"
#include "../session/fundamental.hpp"
#include "../session/conversation.hpp"

#include <boost/asio.hpp>

namespace represents
{
  /**
   * @brief 基于 `http` 协议的服务端http请求转发器
   * @details 用于将客户端的http请求转发到指定的http服务器，并将服务器的响应返回给客户端
   */
  class transponder
  {
    using request = protocol::http::request<boost::beast::http::string_body>;
    using response = protocol::http::response<boost::beast::http::string_body>;
    boost::asio::io_context& _io_context;
    conversation::session_management<request,response> _management;
  }; // end class transponder
} // end namespace represents
