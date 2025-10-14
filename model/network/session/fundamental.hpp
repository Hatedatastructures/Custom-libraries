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
    void renewal_activity() noexcept
    {
      _last_activity = std::chrono::system_clock::now();
    }

    /**
     * @brief 获取会话持续时间（秒）
     * @return 持续时间
     */
    std::chrono::seconds get_duration() const noexcept
    {
      return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - _created_time);
    }

    /**
     * @brief 获取空闲时间（秒）
     * @return 空闲时间
     */
    std::chrono::seconds get_idle_time() const noexcept
    {
      return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now() - _last_activity);
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

    std::chrono::milliseconds _heartbeat_interval{60000}; // 心跳间隔

    bool _enable_heartbeat{true};                         // 启用心跳
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
    {p.to_string()} -> std::same_as<const std::string&>;
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
   * @tparam request_t 请求协议类型
   * @tparam response_t 响应协议类型
   * @warning 模板约束采用`protocol_constraints`concept，确保协议类具有`to_string`和`from_string`方法
   * @note 会话类支持同步和异步请求处理，以及错误处理
   * @details 提供会话管理、状态监控、错误处理等功能
   */
  template <protocol_constraints request_t, protocol_constraints response_t>
  class session : public std::enable_shared_from_this<session<request_t, response_t>>
  {
  public:
    using session_handling_t = session_handling<request_t, response_t>;
    using session_handling_ptr = std::shared_ptr<session_handling_t>;
    using session_ptr = std::shared_ptr<session<request_t, response_t>>;
    session_handling_ptr _session_handling; // 会话处理
  private:

    boost::asio::io_context& _io_context; // 引用IO上下文

    boost::asio::steady_timer _timer; // 定时器

    boost::asio::ip::tcp::socket _socket; // TCP套接字
    std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> _ssl_socket; // SSL套接字

    session_type _type; // 会话类型
    session_config _config; // 会话配置
    session_statistics _statistics; // 会话统计信息
    session_state _state{session_state::DISCONNECTED}; // 会话状态

    std::string _session_id; // 会话ID
    std::string _remote_address; // 远程地址
    std::uint16_t _remote_port{0}; // 远程端口

    mutable std::shared_mutex _state_mutex; // 共享互斥锁

    std::string _received_data; // 读取缓冲区
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
        if(_session_handling->_error_callback)
          _session_handling->_error_callback(this->shared_from_this(),boost::system::error_code(),{e.what()});
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
     * @brief 启动读取数据操作
     */
    void _start_read()
    {
      if(_state == session_state::CONNECTED)
        return ;
      _received_data.resize(1024);
      auto self = this->shared_from_this();
      if(_config._enable_ssl && _ssl_socket)
      {
        auto ssl_function = [self](const boost::system::error_code& ec, std::uint64_t bytes_transferred)
        {
          self->_handle_read(ec, bytes_transferred);
        };
        _ssl_socket->async_read_some(boost::asio::buffer(_received_data), ssl_function);
      }
      else
      {
        auto tcp_function = [self](const boost::system::error_code& ec, std::uint64_t bytes_transferred)
        {
          self->_handle_read(ec, bytes_transferred);
        };
        _socket.async_read_some(boost::asio::buffer(_received_data), tcp_function);
      }
    }
    /**
     * @brief 处理读取数据完成
     * @param ec 错误码
     * @param bytes_transferred 传输字节数
     */
    void _handle_read(const boost::system::error_code& ec, std::uint64_t bytes_transferred)
    {
      if (ec)
      {
        _handle_error(ec);
        return;
      }

      _statistics._bytes_received += bytes_transferred;
      _statistics._messages_received++;
      _statistics.renewal_activity();

      // 处理协议数据
      _process_protocol_data(_received_data);

      // 循环调用
      _start_read();
    }
    /**
     * @brief 处理协议数据
     * @param data 数据
     */
    void _process_protocol_data(std::string data)
    {
      try
      {
        request_t request;
        bool parse_success = false;
        
        // 使用处理器的数据处理方法
        if (_session_handling)
          parse_success = _session_handling->process_data(data, request);
        else
          parse_success = request.from_string(data);
        
        if (parse_success)
        {
          // 验证数据完整性（如果协议支持）
          if constexpr (requires { request.verify_integrity(); })
          {
            if (!request.verify_integrity())
            {
              _handle_error(boost::asio::error::invalid_argument);
              return;
            }
          }
          _process_request(request);
        }
        else
        {
          // 协议解析失败
          _handle_error(boost::asio::error::invalid_argument);
          return;
        }
      }
      catch (const std::exception &e)
      {
        _handle_error(boost::asio::error::invalid_argument);
        return;
      }
    }
    /**
     * @brief 处理请求
     * @param request 请求对象
     */
    void _process_request(const request_t &request)
    {
      if (!_session_handling)
        return;

      auto self = this->shared_from_this();
      
      // 同步处理请求
      auto response = _session_handling->on_request(self, request);
      if (response.has_value())
      { ///                                            问题，响应处理问题
        send_response(response.value());
      }
    }

    /**
     * @brief 处理错误
     * @param ec 错误码
     * @warning 该函数会关闭会话
     */
    void _handle_error(const boost::system::error_code &ec)
    {
      if (_state == session_state::DISCONNECTED || _state == session_state::DISCONNECTING)
        return;

      _set_state(session_state::ERROR_STATE);

      if (_session_handling)
      {
        _session_handling->on_error(this->shared_from_this(), ec);
      }
      close();
    }
    /**
     * @brief 启动心跳定时器
     * @details 心跳定时器用于检测会话是否超时，超时后会触发错误处理
     */
    void _start_heartbeat_timer()
    {
      if (!_config._enable_heartbeat)
        return;

      _timer.expires_after(_config._heartbeat_interval);
      auto self = this->shared_from_this();
      auto timer_function = [self](const boost::system::error_code &ec)
      {
        if(!ec)
          self->_handle_heartbeat();
      };
      _timer.async_wait(timer_function);
    }
    /**
     * @brief 处理心跳
     * @details 检查会话是否超时，超时后会触发错误处理
     */
    void _handle_heartbeat()
    {
      if (_state != session_state::CONNECTED)
        return;

      // 检查空闲时间
      auto idle_time = _statistics.get_idle_time();
      if (idle_time > _config._heartbeat_interval * 2)
      {
        // 超时，关闭连接
        if (_session_handling)
          _session_handling->on_timeout(this->shared_from_this());
        close();
        return;
      }
      _start_heartbeat_timer(); // 继续心跳
    }
  public:
    session(boost::asio::io_context &io_context,session_type type = session_type::TCP_CLIENT,
      const session_config &config = session_config{})
    : _io_context(io_context),_timer(io_context),_socket(io_context), _type(type), _config(config),
     _session_id(_generate_session_id())
    {
      if (_config._enable_ssl)
      {
        _ssl_socket = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
        (io_context, _create_ssl_context());
      }
    }
    session(boost::asio::ip::tcp::socket &&socket,session_type type = session_type::TCP_SERVER,
      const session_config &config = session_config{})
      : _io_context(socket.get_executor().context()),_timer(_io_context), _socket(std::move(socket)),
        _type(type), _config(config), _session_id(_generate_session_id())
    {
      if (_socket.is_open())
      {
        _set_state(session_state::CONNECTED);
        auto endpoint = _socket.remote_endpoint();
        _remote_address = endpoint.address().to_string();
        _remote_port = endpoint.port();
      }

      if (_config._enable_ssl)
      {
        _ssl_socket = std::make_unique<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
        (std::move(_socket), _create_ssl_context());
      }
    }
    ~session()
    {
      close();
    }
    session(const session &) = delete;
    session &operator=(const session &) = delete;

    /**
     * @brief 设置事件处理
     * @param handling 处理类智能指针对象
     */
    void set_handling(session_handling_ptr handling)
    {
      _session_handling = handling;
    }
    /**
     * @brief 获取事件处理
     * @return 处理类智能指针对象
     */
    session_handling_ptr get_handling() const
    {
      return _session_handling;
    }
    /**
     * @brief 创建并设置事件处理
     * @return 处理类智能指针对象
     */
    session_handling_ptr create_handling()
    {
      _session_handling = std::make_shared<session_handling_t>();
      return _session_handling;
    }
    /**
     * @brief 获取会话`ID`
     * @return 会话`ID`
     */
    const std::string &get_session_id() const
    {
      return _session_id;
    }
    /**
     * @brief 获取会话状态
     * @return 会话状态
     */
    session_state get_state() const
    {
      std::lock_guard<std::shared_mutex> lock(_state_mutex);
      return _state;
    }
    /**
     * @brief 获取会话类型
     * @return 会话类型
     */
    session_type get_type() const
    {
      return _type;
    }
    /**
     * @brief 获取远程地址
     * @return 远程地址
     */
    std::string get_remote_address() const
    {
      return _remote_address;
    }
    /**
     * @brief 获取远程端口
     * @return 远程端口
     */
    std::uint16_t get_remote_port() const
    {
      return _remote_port;
    }
    /**
     * @brief 获取统计信息
     * @return 统计信息
     * @note 统计信息为只读，不能修改
     */
    const session_statistics &get_statistics() const noexcept
    {
      return _statistics;
    }
    /**
     * @brief 检查是否已连接
     * @return 是否已连接
     */
    bool is_connected() const noexcept
    {
      return get_state() == session_state::CONNECTED;
    }
    /**
     * @brief 异步连接到远程地址
     * @param host 主机地址
     * @param port 端口
     * @param callback 连接完成回调
     * @warning 回调参数须接受 `boost::system::error_code` 类型参数
     */
    void async_connect(const std::string& host,std::uint16_t port,
      std::function<void(const boost::system::error_code&)> callback = nullptr)
    {
      if (_state != session_state::DISCONNECTED)
      {
        if (callback)
          callback(boost::asio::error::already_connected);
        return;
      }
      _set_state(session_state::CONNECTING);
      _remote_address = host;
      _remote_port = port;

      boost::asio::ip::tcp::resolver resolver(_io_context);
      auto self = this->shared_from_this();

      auto ssl_connect = [self,callback](const boost::system::error_code& ec,const boost::asio::ip::tcp::endpoint&)
      {
        if(ec)
        {
          self->_set_state(session_state::DISCONNECTED);
          if (callback)
            callback(ec);
          return;
        }
        // 连接成功，异步握手
        auto ssl_handshake = [self,callback](const boost::system::error_code& handshake_ec)
        {
          if(handshake_ec)
          {
            self->_set_state(session_state::DISCONNECTED);
            if (callback)
              callback(handshake_ec);
            return;
          }
          self->_set_state(session_state::CONNECTED);
          self->_start_read(); // 启动异步读取
          self->_start_heartbeat_timer(); // 启动心跳定时器
          if(self->_session_handling)
            self->_session_handling->on_connect();
          if (callback)
            callback(boost::system::error_code());
        };
        self->_ssl_socket->async_handshake(boost::asio::ssl::stream_base::client,ssl_handshake);
      };

      auto tcp_connect = [self,callback](const boost::system::error_code& ec,const boost::asio::ip::tcp::endpoint&)
      {
        if(ec)
        {
          self->_set_state(session_state::DISCONNECTED);
          if (callback)
            callback(ec);
          return;
        }
        self->_set_state(session_state::CONNECTED);
        self->_start_read(); // 启动异步读取
        self->_start_heartbeat_timer(); // 启动心跳定时器
        if(self->_session_handling)
          self->_session_handling->on_connect(self);
        if (callback)
          callback(boost::system::error_code());
      };

      auto asynchronous_function = [self,callback,tcp_connect,ssl_connect](const boost::system::error_code& ec,
        boost::asio::ip::tcp::resolver::results_type results)
      {
        if(ec)
        {
          self->_set_state(session_state::DISCONNECTED);
          if (callback)
            callback(ec);
          return;
        }
        if(self->_config._enable_ssl && self->_ssl_socket)
          boost::asio::async_connect(self->_ssl_socket->lowest_layer(),results,ssl_connect);
        else
          boost::asio::async_connect(self->socket,results,tcp_connect);
      };

      resolver.async_resolve(host,std::to_string(port),asynchronous_function);
    }
    /**
     * @brief 启动会话
     * @note 会话启动后，会自动连接到远程地址
     */
    void start()
    {
      if (_state == session_state::CONNECTED)
      {
        if(_config._enable_ssl && _ssl_socket && _type == session_type::SSL_SERVER)
        {
          auto self = this->shared_from_this();
          auto ssl_handshake = [self](const boost::system::error_code& handshake_ec)
          {
            if(handshake_ec)
            {
              self->_set_state(session_state::DISCONNECTED);
              return;
            }
            self->_start_read(); // 启动异步读取
            self->_start_heartbeat_timer(); // 启动心跳定时器
            if(self->_session_handling)
              self->_session_handling->on_connect(self);
          };
          _ssl_socket->async_handshake(boost::asio::ssl::stream_base::server,ssl_handshake);
        }
        else
        {
          _start_read(); // 启动异步读取
          _start_heartbeat_timer(); // 启动心跳定时器
          if(_session_handling)
            _session_handling->on_connect(this->shared_from_this());
        }
      }
    }
    /**
     * @brief 同步发送请求
     * @param request 请求
     * @param callback 发送完成回调
     */
    void send_request(const request_t& request,std::function<void(const boost::system::error_code&)> callback = nullptr)
    {
      if(_state != session_state::CONNECTED)
      {
        if (callback)
          callback(boost::asio::error::not_connected);
        return;
      }
      try
      {
        std::string data = request.to_string();
        auto self = this->shared_from_this();
        if(_config._enable_ssl && _ssl_socket)
        {
          auto ssl_send_function = [self,callback](const boost::system::error_code& ec,std::uint64_t bytes_transferred)
          {
            if(!ec)
            {
              self->_statistics._bytes_sent += bytes_transferred;
              self->_statistics._messages_sent++;
              self->_statistics.renewal_activity();
            }
            else
              self->_handle_error(ec);
            if (callback)
              callback(ec);
          };
          boost::asio::async_write(*_ssl_socket,boost::asio::buffer(data),ssl_send_function);
        }
        else
        {
          auto tcp_send_function = [self,callback](const boost::system::error_code& ec,std::uint64_t bytes_transferred)
          {
            if(!ec)
            {
              self->_statistics._bytes_sent += bytes_transferred;
              self->_statistics._messages_sent++;
              self->_statistics.renewal_activity();
            }
            else
              self->_handle_error(ec);
            if (callback)
              callback(ec);
          };
          boost::asio::async_write(socket,boost::asio::buffer(data),tcp_send_function);
        }
      }
      catch(const std::exception& e)
      {
        if (callback)
          callback(boost::asio::error::invalid_argument);
      }
    }
    /**
     * @brief 同步发送响应
     * @param response 响应对象
     * @param callback 发送完成回调
     */
    void send_response(const response_t& response,std::function<void(const boost::system::error_code&)> callback = nullptr)
    {
      if (_state != session_state::CONNECTED)
      {
        if (callback)
          callback(boost::asio::error::not_connected);
        return;
      }
      try
      {
        std::string data = response.to_string();
        auto self = this->shared_from_this();
        if(_config._enable_ssl && _ssl_socket)
        {
          auto ssl_send_function = [self,callback](const boost::system::error_code& ec,std::uint64_t bytes_transferred)
          {
            if(!ec)
            {
              self->_statistics._bytes_sent += bytes_transferred;
              self->_statistics._messages_sent++;
              self->_statistics.renewal_activity();
            }
            else
              self->_handle_error(ec);
            if (callback)
              callback(ec);
          };
          boost::asio::async_write(*_ssl_socket,boost::asio::buffer(data),ssl_send_function);
        }
        else
        {
          auto tcp_send_function = [self,callback](const boost::system::error_code& ec,std::uint64_t bytes_transferred)
          {
            if(!ec)
            {
              self->_statistics._bytes_sent += bytes_transferred;
              self->_statistics._messages_sent++;
              self->_statistics.renewal_activity();
            }
            else
              self->_handle_error(ec);
            if (callback)
              callback(ec);
          };
          boost::asio::async_write(socket,boost::asio::buffer(data),tcp_send_function);
        }
      }
      catch(const std::exception& e)
      {
        if (callback)
          callback(boost::asio::error::invalid_argument);
      }
    }
    /**
     * @brief 关闭会话
     * @details 关闭会话，释放资源
     */
    void close()
    {
      if(_state == session_state::DISCONNECTED || _state == session_state::DISCONNECTING)
        return;
      _set_state(session_state::DISCONNECTING);
      boost::system::error_code ec;
      _timer.cancel(ec);
      if(_ssl_socket)
        _ssl_socket->lowest_layer().close(ec);
      else
        _socket.close(ec);
      _set_state(session_state::DISCONNECTED);
      if (_session_handling)
        _session_handling->on_disconnected(this->shared_from_this(), ec);
    }
  }; // end class session
} // end namespace fundamental
