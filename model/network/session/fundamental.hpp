/**
 * @file fundamental.hpp
 * @brief 会话辅助类定义
 * @details 提供会话管理、状态监控、错误处理等功能
 */
#pragma once
#include <memory>
#include <string>
#include <mutex>
#include <shared_mutex>

#include "../agreement/json.hpp"
#include "../agreement/auxiliary.hpp"
#include "../agreement/protocol.hpp"
#include "../agreement/conversion.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/pool/object_pool.hpp>

namespace conversation
{
  namespace fundamental
  {
  } // end namespace fundamental
} // end namespace conversation

namespace conversation::fundamental
{

  using request = protocol::request<protocol::request_header>;
  using response = protocol::response<protocol::response_header>;
  /**
   * @brief 会话状态枚举
   */
  enum class session_state : std::uint8_t
  {
    DISCONNECTED,     // 未连接
    CONNECTING,       // 连接中
    CONNECTED,        // 已连接
    DISCONNECTING,    // 断开连接中
    ERROR_STATE       // 错误状态
  };
  /**
   * @brief 会话类型枚举
   */
  enum class session_type : std::uint8_t
  {
    TCP_CLIENT, // TCP客户端
    TCP_SERVER, // TCP服务端
    UDP_CLIENT, // UDP客户端
    UDP_SERVER, // UDP服务端
    SSL_CLIENT, // SSL客户端
    SSL_SERVER  // SSL服务端
  };
  /**
   * @brief 会话事件类型
   */
  enum class session_event : std::uint8_t
  {
    CONNECTED = 0,  // 连接建立
    DISCONNECTED,   // 连接断开
    DATA_RECEIVED,  // 数据接收
    DATA_SENT,      // 数据发送
    ERROR_OCCURRED, // 错误发生
    TIMEOUT         // 超时
  };
  /**
   * @brief 会话统计信息
   */
  struct session_statistics
  {
    std::atomic<std::uint64_t> _bytes_sent{0};            // 发送字节数
    std::atomic<std::uint64_t> _bytes_received{0};        // 接收字节数
    std::atomic<std::uint64_t> _messages_sent{0};         // 发送消息数
    std::atomic<std::uint64_t> _messages_received{0};     // 接收消息数
    std::chrono::system_clock::time_point _created_time;  // 创建时间
    std::chrono::system_clock::time_point _last_activity; // 最后活动时间

    session_statistics() : _created_time(std::chrono::system_clock::now()), 
      _last_activity(std::chrono::system_clock::now()) {}

    /**
     * @brief 更新最后活动时间
     */
    void update_activity() noexcept
    {
      _last_activity = std::chrono::system_clock::now();
    }

    /**
     * @brief 获取会话持续时间（秒）
     * @return 持续时间
     */
    std::chrono::seconds get_duration() const noexcept
    {
      return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - _created_time);
    }

    /**
     * @brief 获取空闲时间（秒）
     * @return 空闲时间
     */
    std::chrono::seconds get_idle_time() const noexcept
    {
      return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now() - _last_activity);
    }
  };

  /**
   * @brief 会话配置
   */
  struct session_config
  {
    std::chrono::milliseconds _read_timeout{30000};       // 读取超时
    std::chrono::milliseconds _write_timeout{30000};      // 写入超时
    std::chrono::milliseconds _connect_timeout{30000};    // 连接超时
    std::chrono::milliseconds _keepalive_interval{60000}; // 心跳间隔

    bool _enable_ssl{false};                              // 启用SSL
    bool _enable_async_processing{true};                  // 启用异步处理

    std::string _ssl_cert_file;                           // SSL证书文件
    std::string _ssl_key_file;                            // SSL私钥文件

    std::size_t _max_buffer_size{65536};    // 最大缓冲区大小
    std::size_t _max_message_size{1048576}; // 最大消息大小
  };

  /**
   * @brief 协议约束概念
   * @details 定义协议类的约束条件，包括to_string和from_string方法
   */
  template <typename protocol_t>
  concept protocol_constraints = requires(protocol_t p,std::string_view str_value)
  {
    {p.to_string()} -> std::same_as<std::string>;
    {p.from_string(str_value)} -> std::same_as<bool>;
  };

  template <protocol_constraints request_t = request, protocol_constraints response_t = response>
  class session;

  /**
   * @brief 会话事件处理类
   * @details 提供会话事件的回调函数接口
   */
  template <protocol_constraints request_t, protocol_constraints response_t>
  class session_handling
  {
  public:
    using session_ptr = std::shared_ptr<session<request_t, response_t>>;

    using connection_callback = std::function<void(session_ptr)>;
    using disconnect_callback = std::function<void(session_ptr, const boost::system::error_code&)>;

    using request_callback = std::function<std::optional<response_t>(session_ptr, const request_t&)>;
    using request_async_callback = std::function<void(session_ptr, const request_t&, std::function<void(std::optional<response_t>)>)>;

    using response_callback = std::function<void(session_ptr, const response_t&)>;
    using request_error_callback = std::function<void(session_ptr, const request_t&, const boost::system::error_code&)>;

    using error_callback = std::function<void(session_ptr, const boost::system::error_code&,const std::string&)>;
    using timeout_callback = std::function<void(session_ptr)>;

  private:
    connection_callback _connection_callback; // 连接回调
    disconnect_callback _disconnect_callback; // 断开连接回调

    request_callback _request_callback; // 请求回调
    request_async_callback _request_async_callback; // 异步请求回调

    response_callback _response_callback; // 响应回调
    request_error_callback _request_error_callback; // 请求错误回调

    error_callback _error_callback; // 错误回调
    timeout_callback _timeout_callback; // 超时回调
  public:
    session_handling() = default;
    ~session_handling() = default;
    /**
     * @brief 设置连接回调
     * @param callback 连接回调函数
     */
    void set_connection_callback(connection_callback callback)
    {
      _connection_callback = std::move(callback);
    }
    /**
     * @brief 设置断开连接回调
     * @param callback 断开连接回调函数
     */
    void set_disconnect_callback(disconnect_callback callback)
    {
      _disconnect_callback = std::move(callback);
    }
    /**
     * @brief 设置请求回调
     * @param callback 请求回调函数
     */
    void set_request_callback(request_callback callback)
    {
      _request_callback = std::move(callback);
    }
    /**
     * @brief 设置异步请求回调
     * @param callback 异步请求回调函数
     */
    void set_request_async_callback(request_async_callback callback)
    {
      _request_async_callback = std::move(callback);
    }
    /**
     * @brief 设置响应回调
     * @param callback 响应回调函数
     */
    void set_response_callback(response_callback callback)
    {
      _response_callback = std::move(callback);
    }
    /**
     * @brief 设置请求错误回调
     * @param callback 请求错误回调函数
     */
    void set_request_error_callback(request_error_callback callback)
    {
      _request_error_callback = std::move(callback);
    }
    /**
     * @brief 设置错误回调
     * @param callback 错误回调函数
     */
    void set_error_callback(error_callback callback)
    {
      _error_callback = std::move(callback);
    }
    /**
     * @brief 设置超时回调
     * @param callback 超时回调函数
     */
    void set_timeout_callback(timeout_callback callback)
    {
      _timeout_callback = std::move(callback);
    }
  }; // end class session_handling
  /**
   * @brief 会话类
   * @details 提供会话管理、状态监控、错误处理等功能
   */
  template <protocol_constraints request_t , protocol_constraints response_t>
  class session : public std::enable_shared_from_this<session<request_t, response_t>>
  {
  public:
    using session_handling_t = session_handling<request_t, response_t>;
    using session_ptr = std::shared_ptr<session<request_t, response_t>>;
    session_handling_t _session_handling; // 会话处理
  private:
    boost::asio::steady_timer _timer; // 定时器

    boost::asio::io_context& _io_context; // 引用IO上下文

    boost::asio::ip::tcp::socket _socket; // TCP套接字
    std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> _ssl_socket; // SSL套接字

    session_type _type; // 会话类型
    session_config _config; // 会话配置
    session_statistics _statistics; // 会话统计信息
    session_state _state{session_state::DISCONNECTED}; // 会话状态

    std::string _session_id; // 会话ID
    std::string _remote_address; // 远程地址
    std::uint16_t _remote_port{0}; // 远程端口

    mutable std::shared_mutex _mutex; // 共享互斥锁

    std::string _read_buffer; // 读取缓冲区
  private:
    /**
     * @brief 生成唯一会话`ID`
     * @return 会话`ID`
     * @note 基于`64`字节随机数据混入后的`SHA256`哈希
     */
    std::string _generate_session_id()
    {
      return encryption::umbrage_hash::SHA256(encryption::mix64());
    }
    /**
     * @brief 创建SSL上下文
     * @return SSL上下文
     * @note 基于会话配置初始化SSL上下文，包括证书验证、密码套件等
     */
    boost::asio::ssl::context _create_ssl_context()
    {
      boost::asio::ssl::context ssl_context(boost::asio::ssl::context::sslv23);
      try
      {
        ssl_context.set_options(boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 
          | boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::single_dh_use);
        if(_type == session_type::SSL_SERVER)
        {
          if(!_config._ssl_cert_file.empty())
            ssl_context.use_certificate_chain_file(_config._ssl_cert_file);
          if(!_config._ssl_key_file.empty())
            ssl_context.use_private_key_file(_config._ssl_key_file, boost::asio::ssl::context::pem);
        }
        else if(_type == session_type::SSL_CLIENT)
        {
          ssl_context.set_verify_mode(boost::asio::ssl::verify_peer);
          ssl_context.set_default_verify_paths();
          
          // 如果提供了客户端证书
          if (!_config._ssl_cert_file.empty())
            ssl_context.use_certificate_chain_file(_config._ssl_cert_file);
          if (!_config._ssl_key_file.empty())
            ssl_context.use_private_key_file(_config._ssl_key_file, boost::asio::ssl::context::pem);
        }
      }
      catch(const std::exception& e)
      {
        if(_session_handling._error_callback)
          _session_handling._error_callback(this->shared_from_this(),boost::system::error_code(),{e.what()});
      }
    }
    /**
     * @brief 设置会话状态
     * @param state 新状态
     */
    void _set_state(session_state state)
    {
      std::lock_guard<std::shared_mutex> lock(_state_mutex);
      _state = state;
    }
    /**
     * @brief 启动读取操作
     */
    void _start_read()
    {
      if(_state == session_state::CONNECTED)
        return ;
      _read_buffer.resize(1024);
      auto self = this->shared_from_this();
      if(_config._enable_ssl && _ssl_socket)
      {
        auto ssl_function = [self](const boost::system::error_code& ec, std::size_t bytes_transferred)
        {
          self->_handle_read(ec, bytes_transferred);
        };
        _ssl_socket->async_read_some(boost::asio::buffer(_read_buffer), ssl_function);
      }
      else
      {
        auto tcp_function = [self](const boost::system::error_code& ec, std::size_t bytes_transferred)
        {
          self->_handle_read(ec, bytes_transferred);
        };
        _socket.async_read_some(boost::asio::buffer(_read_buffer), tcp_function);
      }
    }
  }; // end class session
} // end namespace fundamental
