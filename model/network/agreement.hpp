#pragma once
#include "processing.hpp"
#include "encryption.hpp"
#include <iostream>
#include <vector>
#include <string>
#include <string>
#include <json/json.h>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/beast/http.hpp>
namespace agreement
{
  static constexpr std::uint64_t request_header_size = 12;
  class request_header
  {
  public:
    std::uint32_t magic  = packet_identifier;
    std::uint8_t version = current_version;
    std::uint8_t status = 0;      // 请求端填 0
    std::uint8_t cmd = 0;         // 命令字
    std::uint8_t flags = 0;       // 扩展位
    std::uint32_t body_len = 0;   // 负载长度
    std::uint32_t detection = 0;  // 前 12 字节 CRC

    std::string to_string() const
    {
      std::string output_str(request_header_size, 0);
      std::memcpy(output_str.data(), this, request_header_size);
      return output_str;
    }

    bool to_request_heafer(const std::string &data, const std::uint64_t len)
    {
      if (len < request_header_size)
        return false;
      std::memcpy(this, data.data(), request_header_size);
      if(!verification(data))
        return false;
      if(magic != packet_identifier|| version != current_version || status != 0 )
        return false;
      return true; 
    }
  private:
    bool verification(const std::string &data)
    {
      if (len < request_header_size)
        return false;
      size_t copy_len = std::min(request_header_size, data.size());
      std::memcpy(this, data.data(), copy_len);
      return encryption::CyclicRedundancyCheck32(data.data(),request_header_size) == detection;
    }
  }; //end request_header

} // end agreement

namespace ip
{
  class underground_agreement
  {
    virtual underground_agreement() = 0;
    // virtual void send_agreement(agreement::internal_agreement agreement) = 0;
    virtual void connect(const std::string &ip, uint16_t port, ConnectCallback callback) = 0;
    virtual bool bind(uint16_t port) = 0;
    virtual bool send(const std::string &danetwork_packetta) = 0;
    virtual void set_receive_callback(ReceiveCallback callback) = 0;
    virtual void set_disconnect_callback(DisconnectCallback callback) = 0;

    // 关闭连接
    virtual void close() = 0;
  };
  class udp : public underground_agreement
  {

  }; // end underground_agreement
  class tcp : public underground_agreement
  {

  }; // end tcp
  class http : public underground_agreement
  {

  }; // http
} // end ip