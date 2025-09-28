#include <boost/asio.hpp>
#include <memory>
#include <type_traits>
#include "../module/thread_pool.hpp"
#include <chrono>
#include <functional>
#include "agreement.hpp"


namespace message_packaging
{
  enum class message_direction : std::uint8_t 
  {
    REQUEST,    // 客户端→服务器
    RESPONSE,   // 服务器→客户端
    BIDIRECTIONAL // 双向通信
  };
  class general_packaging
  {
  public:
    virtual ~general_packaging() = default;
    /**
     * @brief 序列化
     * @return `std::string` 序列化后的字符串
     */
    virtual std::string serialize() const = 0;
    /**
     * @brief 序列化为`json`格式
     * @return `boost::json::value` 序列化后的`json`格式数据
     */
    virtual boost::json::value serialize_json() const = 0;
    /**
     * @brief 反序列化
     * @param data 待反序列化的字符串
     * @return `bool` 是否成功反序列化
     */
    virtual bool deserialize(const std::string& data) = 0;
    /**
     * @brief 获取协议类型
     * @return `message_direction` 协议类型
     */
    virtual message_direction  get_protocol_type() const = 0;
    /**
     * @brief 从`json`格式数据反序列化
     * @param data 待反序列化的`json`格式数据
     * @return `bool` 是否成功反序列化
     */
    virtual bool deserialize_json(const boost::json::value& data) = 0;
  }; // general_packaging

  /**
   * @brief 请求包装类
   * @details 该类封装了一个请求，包括请求头和请求体。
   */
  template <class request_t = agreement::request<agreement::request_header>>
  class request_packaging : public general_packaging
  {
    request_t request; 
  public:
    request_packaging() = default;
    request_packaging(const request_t& request_value) : request(request_value) {}
    std::string serialize() const override
    {
      return request.to_string();
    }
    boost::json::value serialize_json() const override
    {
      return agreement::to_json(request);
    }
    bool deserialize(const std::string& data) override
    {
      return request.from_string(data);
    }
    message_direction  get_protocol_type() const override
    {
      return message_direction::REQUEST;
    }
    bool deserialize_json(const boost::json::value& data) override
    {
      return agreement::from_json(data, request);
    }
  }; // request_packaging
  /**
   * @brief 响应包装类
   * @details 该类封装了一个响应，包括响应头和响应体。
   */
  template <class response_t = agreement::response<agreement::response_header>>
  class response_packaging : public general_packaging
  {
    response_t response;
  public:
    response_packaging() = default;
    response_packaging(const response_t& response_value) : response(response_value) {}
    std::string serialize() const override
    {
      return response.to_string();
    }
    boost::json::value serialize_json() const override
    {
      return agreement::to_json(response);
    }
    bool deserialize(const std::string& data) override
    {
      return response.from_string(data);
    }
    message_direction  get_protocol_type() const override
    {
      return message_direction::RESPONSE;
    }
    bool deserialize_json(const boost::json::value& data) override
    {
      return agreement::from_json(data,response);
    }
  }; // response_packaging
} // namespace message_packaging

/**
 * @brief 序列化/反序列化 traits 类
 * @tparam other_t 要序列化/反序列化的类型
 */
template <typename other_t, typename = void>
class serialization_traits
{
public:
  /**
   * @brief 序列化
   * @param value 待序列化的值
   * @return `std::string` 序列化后的字符串
   */
  static std::string serialize(other_t& value) = delete;
  /***
   * @brief 反序列化数据
   * @param data 待反序列化的数据
   * @param value 反序列化后的值
   * @return `bool` 是否成功反序列化
   */
  static bool deserialize(other_t& data, std::string& value) = delete;
};

template <>
class serialization_traits<std::string>
{
public:
  static std::string serialize(std::string& value)  {return value;}
  static bool deserialize(std::string& data, std::string& value)  { value = data; return true;}
};

template <>
class serialization_traits<message_packaging::general_packaging>
{
public:
  static std::string serialize(message_packaging::general_packaging& value)  
  {
    return value.serialize();
  }
  static bool deserialize(message_packaging::general_packaging& data, std::string& value) 
  {
    return data.deserialize(value);
  }
};
//general_packaging 多态类特化
template <typename polymorphism_t>
class serialization_traits<polymorphism_t,std::enable_if_t<std::is_base_of_v<message_packaging::general_packaging, polymorphism_t>>>
{
public:
  // 调用派生类的serialize()（多态生效）
  static std::string serialize(polymorphism_t& value)  
  {
    return value.serialize();
  }
  static bool deserialize(polymorphism_t& data, std::string& value) 
  {
    return data.deserialize(value);
  }
};


// namespace message_function_utility
// {
//   //处理conversation类的类型自动转换函数钩子，方便直接进行发送数据
// } // namespace message_function_utility

namespace conversation_management
{
  using function_type = std::function<void(boost::asio::ip::tcp::socket &)>;
  //封装异步传输回调函数
  using transmission_callback = std::function<void(boost::system::error_code, std::uint64_t)>;

  /**
   * @brief 网络连接会话类
   * @details 该类封装了一个网络连接会话，包括会话的IP地址、端口号、套接字、会话开始时间等信息。
   * @warning 该类的实例化对象只能在`io_context`的线程中使用，否则会导致未定义行为。
   */
  class conversation
  {
    std::uint64_t _total_bytes_sent = 0;
    std::uint64_t _total_bytes_received = 0;

    std::string _ip;
    std::uint16_t _port;
    boost::asio::ip::tcp::socket _socket;
    std::chrono::system_clock::time_point _start_time;
    std::function<void(const char* )> _exception_callback;
  private:

    bool socket_status()
    {
      if(this->_socket.is_open())
        return true;
      return false;
    }

    std::uint64_t do_transmission(const std::string& transmission_value)
    {
      if(!socket_status() || transmission_value.empty())
        return 0;
      return boost::asio::write(this->_socket, boost::asio::buffer(transmission_value));
    }

    bool do_transmission_async(const std::string& transmission_value, transmission_callback callback)
    {
      if(!socket_status() || transmission_value.empty() || !callback)
        return false;
      boost::asio::async_write(this->_socket, boost::asio::buffer(transmission_value), callback);
      return true;
    }

    std::uint64_t do_acceptance(std::string& received_value)
    {
      if(!socket_status())
        return 0;
      return boost::asio::read(this->_socket, boost::asio::buffer(received_value));
    }

    bool do_acceptance_async(std::string& received_value, transmission_callback callback)
    {
      if(!socket_status() || !callback)
        return false;
      boost::asio::async_read(this->_socket, boost::asio::buffer(received_value), callback);
      return true;
    }
  public:
    ~conversation()                     { close(); }
    std::string remote_ip() const       { return this->_ip;   }
    std::uint16_t remote_port() const   { return this->_port; }
    conversation(boost::asio::ip::tcp::socket&& socket) 
    :_socket(std::move(socket)), _start_time(std::chrono::system_clock::now()) 
    {
      this->_port = _socket.remote_endpoint().port();
      this->_ip = _socket.remote_endpoint().address().to_string();
    }
    /**
     * @brief 同步传输数据
     * @tparam transmission_data_type 传输数据的类型
     * @param transmission_value 传输数据
     * @return `std::uint64_t` 传输数据的字节数
     */
    template <typename transmission_data_type>
    std::uint64_t transmission(transmission_data_type& transmission_value)
    {
      try
      {
        return do_transmission(serialization_traits<transmission_data_type>::serialize(transmission_value));
      }
      catch(const std::exception& e)
      {
        if(_exception_callback)
          _exception_callback(e.what());
        return 0;
      }
    }
    /**
     * @brief 异步传输数据
     * @tparam transmission_data_type 传输数据的类型
     * @param transmission_value 传输数据
     * @param callback 传输完成回调函数
     * @return `bool` `socket` 是否正常打开连接
     */
    template <typename transmission_data_type>
    bool transmission_async(transmission_data_type& transmission_value, transmission_callback callback)
    {
      try
      {
        return do_transmission_async(serialization_traits<transmission_data_type>::serialize(transmission_value), callback);
      }
      catch(const std::exception& e)
      {
        if(_exception_callback)
          _exception_callback(e.what());
        return false;
      }
    }
    /**
     * @brief 同步接收数据
     * @tparam transmission_data_type 接收数据的类型
     * @param transmission_value 接收数据
     * @return `std::uint64_t` 接收数据的字节数
     */
    template <typename transmission_data_type>
    std::uint64_t acceptance(transmission_data_type& transmission_value)
    {
      try
      {
        std::string received_value;
        std::uint64_t acceptance_string_len = do_acceptance(received_value);
        if(acceptance_string_len > 0)
          serialization_traits<transmission_data_type>::deserialize(acceptance_string_len,transmission_value);
        return acceptance_string_len;
      }
      catch(const std::exception& e)
      {
        if(_exception_callback)
          _exception_callback(e.what());
        return 0;
      }
    }
    /**
     * @brief 异步接收数据
     * @tparam transmission_data_type 接收数据的类型
     * @param transmission_value 接收数据
     * @param callback 接收完成回调函数
     * @return `std::uint64_t` 接收数据的字节数
     */
    template <typename transmission_data_type>
    bool acceptance_async(transmission_data_type& transmission_value, transmission_callback callback)
    {
      try
      {
        std::string received_value;
        bool acceptance_value = do_acceptance_async(received_value,callback);
        if(acceptance_value)
          serialization_traits<transmission_data_type>::deserialize(received_value,transmission_value);
        return acceptance_value;
      }
      catch(const std::exception& e)
      {
        if(_exception_callback)
          _exception_callback(e.what());
        return false;
      }
    }
    void close()
    {
      if(socket_status())
        this->_socket.close();
    }
  }; // conversation
  class conversation_pool 
  {

  };
} // namespace conversation_management