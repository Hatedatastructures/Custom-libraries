// #include <boost/asio.hpp>
// #include <memory>
// #include <type_traits>
// #include "../module/thread_pool.hpp"
// #include <chrono>
// #include <functional>
// #include "agreement.hpp"

// namespace message_packaging
// {
//   enum class message_direction : std::uint8_t
//   {
//     REQUEST,    // 客户端→服务器
//     RESPONSE,   // 服务器→客户端
//     BIDIRECTIONAL // 双向通信
//   };
//   class general_packaging
//   {
//   public:
//     virtual ~general_packaging() = default;
//     /**
//      * @brief 序列化
//      * @return `std::string` 序列化后的字符串
//      */
//     virtual std::string serialize() const = 0;
//     /**
//      * @brief 序列化为`json`格式
//      * @return `boost::json::value` 序列化后的`json`格式数据
//      */
//     virtual boost::json::value serialize_json() const = 0;
//     /**
//      * @brief 反序列化
//      * @param data 待反序列化的字符串
//      * @return `bool` 是否成功反序列化
//      */
//     virtual bool deserialize(const std::string& data) = 0;
//     /**
//      * @brief 获取协议类型
//      * @return `message_direction` 协议类型
//      */
//     virtual message_direction  get_protocol_type() const = 0;
//     /**
//      * @brief 从`json`格式数据反序列化
//      * @param data 待反序列化的`json`格式数据
//      * @return `bool` 是否成功反序列化
//      */
//     virtual bool deserialize_json(const boost::json::value& data) = 0;
//   }; // general_packaging

//   /**
//    * @brief 请求包装类
//    * @details 该类封装了一个请求，包括请求头和请求体。
//    */
//   template <class request_t = agreement::request<agreement::request_header>>
//   class request_packaging : public general_packaging
//   {
//     request_t request;
//   public:
//     request_packaging() = default;
//     request_packaging(const request_t& request_value) : request(request_value) {}
//     std::string serialize() const override
//     {
//       return request.to_string();
//     }
//     boost::json::value serialize_json() const override
//     {
//       return agreement::to_json(request);
//     }
//     bool deserialize(const std::string& data) override
//     {
//       return request.from_string(data);
//     }
//     message_direction  get_protocol_type() const override
//     {
//       return message_direction::REQUEST;
//     }
//     bool deserialize_json(const boost::json::value& data) override
//     {
//       return agreement::from_json(data, request);
//     }
//   }; // request_packaging
//   /**
//    * @brief 响应包装类
//    * @details 该类封装了一个响应，包括响应头和响应体。
//    */
//   template <class response_t = agreement::response<agreement::response_header>>
//   class response_packaging : public general_packaging
//   {
//     response_t response;
//   public:
//     response_packaging() = default;
//     response_packaging(const response_t& response_value) : response(response_value) {}
//     std::string serialize() const override
//     {
//       return response.to_string();
//     }
//     boost::json::value serialize_json() const override
//     {
//       return agreement::to_json(response);
//     }
//     bool deserialize(const std::string& data) override
//     {
//       return response.from_string(data);
//     }
//     message_direction  get_protocol_type() const override
//     {
//       return message_direction::RESPONSE;
//     }
//     bool deserialize_json(const boost::json::value& data) override
//     {
//       return agreement::from_json(data,response);
//     }
//   }; // response_packaging
// } // namespace message_packaging

// /**
//  * @brief 序列化/反序列化 traits 类
//  * @tparam other_t 要序列化/反序列化的类型
//  */
// template <typename other_t, typename = void>
// class serialization_traits
// {
// public:
//   /**
//    * @brief 序列化
//    * @param value 待序列化的值
//    * @return `std::string` 序列化后的字符串
//    */
//   static std::string serialize(const other_t& value) = delete;
//   /***
//    * @brief 反序列化数据
//    * @param data 待反序列化的数据
//    * @param value 反序列化后的值
//    * @return `bool` 是否成功反序列化
//    */
//   static bool deserialize(other_t& data, std::string& value) = delete;
// };

// template <>
// class serialization_traits<std::string>
// {
// public:
//   static std::string serialize(const std::string& value)  {return value;}
//   static bool deserialize(std::string& data, std::string& value)  { value = data; return true;}
// };

// template <>
// class serialization_traits<message_packaging::general_packaging>
// {
// public:
//   static std::string serialize(const message_packaging::general_packaging& value)
//   {
//     return value.serialize();
//   }
//   static bool deserialize(message_packaging::general_packaging& data, std::string& value)
//   {
//     return data.deserialize(value);
//   }
// };
// //general_packaging 多态类特化
// template <typename polymorphism_t>
// class serialization_traits<polymorphism_t,std::enable_if_t<std::is_base_of_v<message_packaging::general_packaging, polymorphism_t>>>
// {
// public:
//   // 调用派生类的serialize()（多态生效）
//   static std::string serialize(const polymorphism_t& value)
//   {
//     return value.serialize();
//   }
//   static bool deserialize(polymorphism_t& data, std::string& value)
//   {
//     return data.deserialize(value);
//   }
// };

// // namespace message_function_utility
// // {
// //   //处理conversation类的类型自动转换函数钩子，方便直接进行发送数据
// // } // namespace message_function_utility

// namespace conversation_management
// {
//   using function_type = std::function<void(boost::asio::ip::tcp::socket &)>;
//   //封装异步传输回调函数
//   using transmission_callback = std::function<void(boost::system::error_code, std::uint64_t)>;

//   /**
//    * @brief 网络连接会话类
//    * @details 该类封装了一个网络连接会话，包括会话的IP地址、端口号、套接字、会话开始时间等信息。
//    * @warning 该类的实例化对象只能在`io_context`的线程中使用，否则会导致未定义行为。
//    */
//   class conversation
//   {
//     std::uint64_t _total_bytes_sent = 0;
//     std::uint64_t _total_bytes_received = 0;

//     std::string _ip;
//     std::uint16_t _port;
//     boost::asio::ip::tcp::socket _socket;
//     std::chrono::system_clock::time_point _start_time;
//     std::function<void(const char* )> _exception_callback;
//   private:

//     bool socket_status()
//     {
//       if(this->_socket.is_open())
//         return true;
//       return false;
//     }

//     std::uint64_t do_transmission(const std::string& transmission_value)
//     {
//       if(!socket_status() || transmission_value.empty())
//         return 0;
//       return boost::asio::write(this->_socket, boost::asio::buffer(transmission_value));
//     }

//     bool do_transmission_async(const std::string& transmission_value, transmission_callback callback)
//     {
//       if(!socket_status() || transmission_value.empty() || !callback)
//         return false;
//       boost::asio::async_write(this->_socket, boost::asio::buffer(transmission_value), callback);
//       return true;
//     }

//     std::uint64_t do_acceptance(std::string& received_value)
//     {
//       if(!socket_status())
//         return 0;
//       return boost::asio::read(this->_socket, boost::asio::dynamic_buffer(received_value));
//     }

//     bool do_acceptance_async(std::string& received_value, transmission_callback callback)
//     {
//       if(!socket_status() || !callback)
//         return false;
//       boost::asio::async_read(this->_socket, boost::asio::dynamic_buffer(received_value), callback);
//       if(received_value.empty())
//         return false;
//       return true;
//     }
//   public:
//     ~conversation()                     { close(); }
//     std::string remote_ip() const       { return this->_ip;   }
//     std::uint16_t remote_port() const   { return this->_port; }
//     conversation(boost::asio::ip::tcp::socket&& socket)
//     :_socket(std::move(socket)), _start_time(std::chrono::system_clock::now())
//     {
//       this->_port = _socket.remote_endpoint().port();
//       this->_ip = _socket.remote_endpoint().address().to_string();
//     }
//     /**
//      * @brief 同步传输数据
//      * @tparam transmission_data_type 传输数据的类型
//      * @param transmission_value 传输数据
//      * @return `std::uint64_t` 传输数据的字节数
//      */
//     template <typename transmission_data_type>
//     std::uint64_t transmission(transmission_data_type& transmission_value)
//     {
//       try
//       {
//         return do_transmission(serialization_traits<transmission_data_type>::serialize(transmission_value));
//       }
//       catch(const std::exception& e)
//       {
//         if(_exception_callback)
//           _exception_callback(e.what());
//         return 0;
//       }
//     }
//     /**
//      * @brief 异步传输数据
//      * @tparam transmission_data_type 传输数据的类型
//      * @param transmission_value 传输数据
//      * @param callback 传输完成回调函数
//      * @return `bool` `socket` 是否正常打开连接
//      */
//     template <typename transmission_data_type>
//     bool transmission_async(transmission_data_type& transmission_value, transmission_callback callback)
//     {
//       try
//       {
//         return do_transmission_async(serialization_traits<transmission_data_type>::serialize(transmission_value), callback);
//       }
//       catch(const std::exception& e)
//       {
//         if(_exception_callback)
//           _exception_callback(e.what());
//         return false;
//       }
//     }
//     /**
//      * @brief 同步接收数据
//      * @tparam transmission_data_type 接收数据的类型
//      * @param transmission_value 接收数据
//      * @return `std::uint64_t` 接收数据的字节数
//      */
//     template <typename transmission_data_type>
//     std::uint64_t acceptance(transmission_data_type& transmission_value)
//     {
//       try
//       {
//         std::string received_value;
//         std::uint64_t acceptance_string_len = do_acceptance(received_value);
//         if(acceptance_string_len > 0)
//           serialization_traits<transmission_data_type>::deserialize(received_value,transmission_value);
//         return acceptance_string_len;
//       }
//       catch(const std::exception& e)
//       {
//         if(_exception_callback)
//           _exception_callback(e.what());
//         return 0;
//       }
//     }
//     /**
//      * @brief 异步接收数据
//      * @tparam transmission_data_type 接收数据的类型
//      * @param transmission_value 接收数据
//      * @param callback 接收完成回调函数
//      * @return `bool` 是否正常打开连接
//      */
//     template <typename transmission_data_type>
//     bool acceptance_async(transmission_data_type& transmission_value, transmission_callback callback)
//     {
//       try
//       {
//         auto received_value = std::make_shared<std::string>();
//         auto wrapped_callback = [callback,received_value,&transmission_value](boost::system::error_code ec, std::size_t len)
//         {
//           if(!ec)
//             serialization_traits<transmission_data_type>::deserialize(*received_value,transmission_value);
//           callback(ec, len);
//         };
//         return do_acceptance_async(*received_value, wrapped_callback);
//         // std::string received_value;
//         // bool acceptance_value = do_acceptance_async(received_value,callback);
//         // if(acceptance_value)
//         //   serialization_traits<transmission_data_type>::deserialize(received_value,transmission_value);
//         // return acceptance_value;
//       }
//       catch(const std::exception& e)
//       {
//         if(_exception_callback)
//           _exception_callback(e.what());
//         return false;
//       }
//     }
//     void close()
//     {
//       if(socket_status())
//         this->_socket.close();
//     }
//   }; // conversation
//   class conversation_pool
//   {

//   };
// } // namespace conversation_management
bool protocol::response_header::from_string(std::string_view data)
{
  if (data.empty())
    return false;
  _headers.clear();
  std::size_t pos = 0;

  if (const auto le = data.find("\r\n", pos); le == std::string_view::npos)
    return false;
  else
  {
    std::vector<std::string_view> parts;
    for (std::size_t i = pos, s = pos; i <= le; ++i)
    {
      if (i == le || data[i] == ' ')
      { 
        if (i > s)
          parts.push_back(data.substr(s, i - s));
        s = i + 1;
      }
    }
    if (parts.size() < 6)
      return false;

    auto parse = [](std::string_view sv, auto &out)
    {
      return std::from_chars(sv.data(), sv.data() + sv.size(), out).ec == std::errc{};
    };

 
    if (!parse(parts[0], _version) || !parse(parts[1], _status_code))
      return false;
    _status_message = std::string(parts[2]);
    std::uint8_t ctype_val;
    if (!parse(parts[3], ctype_val) || !parse(parts[4], _checksum_value) ||
        !parse(parts[5], _content_length))
      return false;
    _checksum_type = static_cast<checksum_type>(ctype_val);
    pos = le + 2;
  }


  auto trim = [](std::string_view &sv)
  {
    const auto start = sv.find_first_not_of(" \t");
    if (start == std::string_view::npos)
    {
      sv = "";
      return;
    }
    const auto end = sv.find_last_not_of(" \t");
    sv = sv.substr(start, end - start + 1);
  };

  for (; pos < data.size(); pos = le + 2)
  {
    if (const auto le = data.find("\r\n", pos); le == std::string_view::npos)
      break;
    else
    {
      std::string_view line = data.substr(pos, le - pos);
      if (line.empty())
        break;

      if (const auto colon = line.find(':'); colon != std::string_view::npos)
      {
        std::string_view k = line.substr(0, colon), v = line.substr(colon + 1);
        trim(k);
        trim(v);

        std::string key(k), val(v);
        if (key == "Server")
          _server = val;
        else if (key == "Timestamp")
        {
          std::int64_t ts;
          if (parse(val, ts))
            _timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ts));
        }
        else
          _headers[key] = val;
      }
    }
  }

  return true;
}
