#pragma once
#include "processing.hpp"
#include "encryption.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <string>
#include <format>
#include <json/json.h>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
namespace agreement
{
  class request_header
  {
  public:
    std::string method; // 请求方法（如"GET"、"POST"）
    std::uint32_t verification_code; // 校验码
    std::uint64_t headers_string_len; // 头部字段字节
    std::unordered_map<std::string, std::string> headers; // 头部字段

    request_header() {headers.reserve(12);}

    std::string to_string() const
    {
      std::string out = std::format("{} {}\r\n", method, verification_code);
      for (const auto &[key, value] : headers)
      {
        out += std::format("{}: {}\r\n", key, value);
      }
      out += "\r\n";
      return out;
    }
    std::uint32_t calculation()
    {
      verification_code = encryption::CyclicRedundancyCheck32(to_string(),headers_string_len);
      return verification_code;
    }
    bool verification() const
    {
      return verification_code == encryption::CyclicRedundancyCheck32(to_string(),headers_string_len);
    }
    bool from_string(const std::string &request_header_string)
    {
      std::stringstream temporary_handling(request_header_string);
      std::string line;
      if(!std::getline(temporary_handling, line))
        return false;
      if(!line.empty() && line.back() == '\r')
        line.pop_back();
      std::stringstream first_line_stream(line);
      if(!(first_line_stream >> method >> verification_code)) 
        return false;
      headers.clear();
      while (std::getline(temporary_handling, line)) 
      {
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        if (line.empty())
          break;
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos) 
          return false;
        std::string key = line.substr(0, colon_pos);
        size_t value_start = colon_pos + 1;
        while (value_start < line.size() && line[value_start] == ' ') 
          value_start++;
        std::string value = line.substr(value_start);
        headers[key] = value;
      }
      headers_string_len = to_string().size();
      return true;
    }
    std::string& operator[](const std::string &key)
    {
      auto it = headers.find(key);
      if (it != headers.end())
      {
        return it->second;
      }
      auto [create_iterator, create_logo] = headers.try_emplace(key, std::string());
      return create_iterator->second;
    }
    const std::string& at(const std::string& key) const
    {
      auto it = headers.find(key);
      if (it == headers.end()) throw std::out_of_range("key not found");
      return it->second;
    }
  };
  // template <class request_header_t = request_header>
  // class request
  // {
  // public:
  //   request_header_t header;
  //   std::string streaming_message_body;                   // 消息体

  //   //序列化：将对象转换为string（格式：方法 头部键: 值\r\n 消息体）
  //   std::string to_string() const
  //   {
  //     std::stringstream ss;
  //     ss << method << " " << path << "\r\n";
  //     for (const auto &[key, value] : headers)
  //     {
  //       ss << key << ": " << value << "\r\n";
  //     }
  //     ss << "\r\n";
  //     ss << streaming_message_body;
  //     return ss.str();
  //   }

  //   /**
  //    * 反序列化：从string恢复对象（解析string中的数据到成员变量）
  //    * @param data 输入的字符串（需符合序列化格式）
  //    * @return 解析成功返回true，失败返回false
  //    */
  //   bool from_string(const std::string &data)
  //   {
  //     std::istringstream ss(data);
  //     std::string line;

  //     if (!std::getline(ss, line))
  //       return false;
  //     if (!line.empty() && line.back() == '\r')
  //       line.pop_back();
  //     std::istringstream req_line_ss(line);
  //     if (!(req_line_ss >> method >> path))
  //       return false;
  //     headers.clear();
  //     while (std::getline(ss, line))
  //     {
  //       if (!line.empty() && line.back() == '\r')
  //         line.pop_back();
  //       if (line.empty())
  //         break;
  //       size_t colon_pos = line.find(':');
  //       if (colon_pos == std::string::npos)
  //         return false;
  //       std::string key = line.substr(0, colon_pos);
  //       size_t value_pos = colon_pos + 1;
  //       while (value_pos < line.size() && line[value_pos] == ' ')
  //         value_pos++;
  //       std::string value = line.substr(value_pos);
  //       headers[key] = value;
  //     }
  //     std::stringstream body_ss;
  //     body_ss << ss.rdbuf();
  //     streaming_message_body = body_ss.str();

  //     return true;
  //   }
  // }; // end request

  class response
  {
  public:
    std::uint32_t status_code = 200;                      // 状态码（如200、404）
    std::string status_msg;                               // 状态描述（如"OK"、"Not Found"）
    std::unordered_map<std::string, std::string> headers; // 头部字段
    std::string body;                                     // 消息体

    /**
     * 序列化：将对象转换为string（格式：状态码 描述\r\n头部键: 值\r\n...\r\n\r\n消息体）
     */
    std::string to_string() const
    {
      std::stringstream ss;
      // 写入状态行
      ss << status_code << " " << status_msg << "\r\n";
      // 写入头部
      for (const auto &[key, value] : headers)
      {
        ss << key << ": " << value << "\r\n";
      }
      // 头部与消息体分隔符
      ss << "\r\n";
      // 写入消息体
      ss << body;
      return ss.str();
    }

    /**
     * 反序列化：从string恢复对象
     * @param data 输入的字符串（需符合序列化格式）
     * @return 解析成功返回true，失败返回false
     */
    bool from_string(const std::string &data)
    {
      std::istringstream ss(data);
      std::string line;

      if (!std::getline(ss, line))
        return false;
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      std::istringstream status_line_ss(line);
      if (!(status_line_ss >> status_code))
        return false;
      std::getline(status_line_ss, status_msg);
      status_msg = status_msg.substr(status_msg.find_first_not_of(" "));
      headers.clear();
      while (std::getline(ss, line))
      {
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        if (line.empty())
          break;
        size_t colon_pos = line.find(':');
        if (colon_pos == std::string::npos)
          return false;
        std::string key = line.substr(0, colon_pos);
        size_t value_pos = colon_pos + 1;
        while (value_pos < line.size() && line[value_pos] == ' ')
          value_pos++;
        std::string value = line.substr(value_pos);
        headers[key] = value;
      }
      std::stringstream body_ss;
      body_ss << ss.rdbuf();
      body = body_ss.str();
      return true;
    }
  };

} // end agreement

// namespace ip
// {
//   class underground_agreement
//   {
//     virtual underground_agreement() = 0;
//     // virtual void send_agreement(agreement::internal_agreement agreement) = 0;
//     virtual void connect(const std::string &ip, uint16_t port, ConnectCallback callback) = 0;
//     virtual bool bind(uint16_t port) = 0;
//     virtual bool send(const std::string &danetwork_packetta) = 0;
//     virtual void set_receive_callback(ReceiveCallback callback) = 0;
//     virtual void set_disconnect_callback(DisconnectCallback callback) = 0;

//     // 关闭连接
//     virtual void close() = 0;
//   };
//   class udp : public underground_agreement
//   {
//     boost::asio::ip::udp::socket socket;
//   }; // end underground_agreement
//   class tcp : public underground_agreement
//   {
//     boost::asio::ip::tcp::acceptor acceptor;
//   }; // end tcp
//   class http : public underground_agreement
//   {

//   }; // http
// } // end ip