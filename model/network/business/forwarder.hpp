#pragma once
#include "../agreement/http.hpp"
#include "../session/fundamental.hpp"
#include "../session/conversation.hpp"

#include <boost/asio.hpp>

namespace represents
{
  /**
   * @brief 基于 `http` 协议的服务端http请求转发器(代理)
   * @details 用于将客户端的http请求转发到指定的http服务器，并将服务器的响应返回给客户端
   */
  struct transponder_config
  {
    std::string _target_host; // 目标主机
    std::uint16_t _target_port; // 目标端口
  };
  template<class request_t, class response_t>
  class transponder
  {
    boost::asio::io_context& _io_context; // io上下文
    conversation::session_management<request_t,response_t> _management; // 会话管理
  public:
    transponder(boost::asio::io_context& io_context)
    : _io_context(io_context), _management(io_context) {}
    /**
     * @brief 启动转发器
     * @return true 启动成功
     * @return false 启动失败
     */
    bool start()
    {
      return _management.start();
    }
    /**
     * @brief 停止转发器
     * @return true 停止成功
     * @return false 停止失败
     */
    bool stop()
    {
      return _management.stop();
    }
  }; // end class transponder
} // end namespace represents
