#include <boost/asio.hpp>
#include <memory>
#include "module/thread_pool.hpp"
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
    virtual std::string serialize() const = 0;
    virtual boost::json::value serialize_json() const = 0;
    virtual bool deserialize(const std::string& data) = 0;
    virtual message_direction  get_protocol_type() const = 0;
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
    
  }; // request_packaging

} // namespace message_packaging
namespace conversation_management
{
  using function_type = std::function<void(boost::asio::ip::tcp::socket &)>;
  // using agreement_type = agreement::request
  class conversation
  {
  private:
    std::string _ip;
    std::uint16_t _port;
    boost::asio::ip::tcp::socket _socket;
    std::chrono::system_clock::time_point _start_time;
  public:
    conversation(boost::asio::ip::tcp::socket socket) : _start_time(std::chrono::system_clock::now()) 
    {
      this->_socket = std::move(socket);
      this->_port = socket.remote_endpoint().port();
      this->_ip = socket.remote_endpoint().address().to_string();
    }
    void transmission(const std::string& data) // 传输数据
    {
      agreement::request request;
      
      
      boost::asio::write(this->_socket, boost::asio::buffer(request_str));
    }

    void incoming()
    {

    }
  }; // conversation
  class conversation_pool
  {

  };
} // namespace conversation_management