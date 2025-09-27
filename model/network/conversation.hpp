#include <boost/asio.hpp>
#include <memory>
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
      return agreement::from_json(request, data);
    }
  }; // request_packaging
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
      return agreement::from_json(response, data);
    }
  }; // response_packaging
} // namespace message_packaging

/**
 * @brief 序列化/反序列化 traits 类
 * @tparam other_t 要序列化/反序列化的类型
 */
template <typename other_t>
class serialization_traits
{
public:
  /**
   * @brief 序列化
   * @param value 待序列化的值
   * @return `std::string` 序列化后的字符串
   */
  static std::string serialize(other_t& value) = delete;
  /**
   * @brief 反序列化数据
   * @param data 待反序列化的
   * @return `std::string` 反序列化后的字符串
   */
  static std::string deserialize(other_t& data) = delete;
};

template <>
class serialization_traits<std::string>
{
public:
  static std::string serialize(std::string& value)  {return value;}
  static std::string deserialize(std::string& data) {return data;}
};

template <>
class serialization_traits<message_packaging::general_packaging>
{
public:
  static std::string serialize(message_packaging::general_packaging& value)  
  {
    return value.serialize();
  }
  static std::string deserialize(message_packaging::general_packaging& data) 
  {
    std::string value;
    if(data.deserialize(value))
      return value;
    return "";
  }
};

namespace message_function_utility
{
  //处理conversation类的类型自动转换函数钩子，方便直接进行发送数据
} // namespace message_function_utility

namespace conversation_management
{
  using function_type = std::function<void(boost::asio::ip::tcp::socket &)>;

  using transmission_callback = std::function<void(boost::system::error_code, std::uint64_t)>;

  class conversation
  {
    std::string _ip;
    std::uint16_t _port;
    boost::asio::ip::tcp::socket _socket;
    std::chrono::system_clock::time_point _start_time;

  private:

    bool socket_status()
    {
      if(this->_socket.is_open())
        return true;
      return false;
    }

    std::uint64_t internal_transmission(const std::string& transmission_value)
    {
      if(!socket_status() || transmission_value.empty())
        return 0;
      return boost::asio::write(this->_socket, boost::asio::buffer(transmission_value));
    }

    std::uint64_t internal_transmission_async(const std::string& transmission_value, transmission_callback callback)
    {
      if(!socket_status() || transmission_value.empty())
        return 0;
      if(!callback)
        boost::asio::async_write(this->_socket, boost::asio::buffer(transmission_value));
      else
        boost::asio::async_write(this->_socket, boost::asio::buffer(transmission_value), callback);
    }

    std::uint64_t internal_acceptance(std::string& transmission_value)
    {
      if(!socket_status() || transmission_value.empty())
        return 0;
      return boost::asio::read(this->_socket, boost::asio::buffer(transmission_value));
    }

  public:
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
      return internal_transmission(serialization_traits<transmission_data_type>::serialize(transmission_value));
    }
    /**
     * @brief 异步传输数据
     * @tparam transmission_data_type 传输数据的类型
     * @param transmission_value 传输数据
     * @param callback 传输完成回调函数
     * @return `std::uint64_t` 传输数据的字节数
     */
    template <typename transmission_data_type>
    std::uint64_t transmission_async(transmission_data_type& transmission_value, transmission_callback callback)
    {
      return internal_transmission_async(serialization_traits<transmission_data_type>::serialize(transmission_value), callback);
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
      return internal_acceptance(serialization_traits<transmission_data_type>::deserialize(transmission_value));
    }
    std::string remote_ip() const
    {
      return this->_ip;
    }
    std::uint16_t remote_port() const
    {
      return this->_port;
    }
    void close()
    {
      if(socket_status())
        this->_socket.close();
    }
    ~conversation()
    {
      close();
    }
  }; // conversation
  class conversation_pool 
  {

  };
} // namespace conversation_management