#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <concepts> //模板约束
#include <algorithm>
#include <charconv>
#include <cctype>
#include <format>
#include <cstdint>
#include <utility>
#include <boost/json.hpp>
#include "./json.hpp"
#include "./auxiliary.hpp"
#include "./conversion.hpp"
#include "../crypt/encryption.hpp"

namespace protocol
{
  /**
   * @brief 协议头约束
   * @details 定义了协议头的基础接口，要求实现序列化、反序列化、校验和计算等功能
   */
  template <typename request_header_t>
  concept header_constraint = requires(request_header_t header,std::string_view data,const json& json_object,std::string_view content) 
  {
    { header.to_string() } -> std::same_as<std::string>;
    { header.from_string(data) } -> std::same_as<bool>;
    { header.to_json() } -> std::same_as<json>;
    { header.from_json(json_object) } -> std::same_as<bool>;
    { header.verify_integrity(content) } -> std::same_as<bool>;
    { header.calculate_and_set_checksum(content) } -> std::same_as<std::uint32_t>;
  };
  /**
   * @brief `TCP`协议请求头类
   * @details 实现`TCP`协议的请求头，支持自定义方法和高性能解析
   */
  class request_header : public auxiliary::protocol_header
  {
  private:
    std::string _method;                              // 请求方法
    std::string _target;                              // 请求目标
    std::string _user_agent;                          // 用户代理
    std::chrono::system_clock::time_point _timestamp; // 时间戳
  private:
    /**
     * @brief 序列化头部字段为字符串
     * @param out 输出字符串
     */
    void _serialize_headers_to_string(std::string &out) const
    {
      std::vector<std::string> keys;
      keys.reserve(_headers.size());
      for (const auto &[key, value] : _headers)
        keys.push_back(key);
      std::sort(keys.begin(), keys.end());
      for (const std::string &key : keys)
      {
        const std::string &value = _headers.at(key);
        out.append(key);
        out.append(": ");
        out.append(value);
        out.append("\r\n");
      }
    }
  public:
    request_header()
    {
      _timestamp = std::chrono::system_clock::now();
      _protocol_type = auxiliary::protocol_type::CUSTOM_TCP;
    }

    const std::string &get_method() const noexcept { return _method; }
    void set_method(const std::string &method) { _method = method; }

    const std::string &get_target() const noexcept { return _target; }
    void set_target(const std::string &target) { _target = target; }

    const std::string &get_user_agent() const noexcept { return _user_agent; }
    void set_user_agent(const std::string &user_agent) { _user_agent = user_agent; }

    const std::chrono::system_clock::time_point &get_timestamp() const noexcept { return _timestamp; }
    void set_timestamp(const std::chrono::system_clock::time_point &timestamp) { _timestamp = timestamp; }

    /**
     * @brief 序列化为字符串
     * @return 序列化后的字符串
     */
    std::string to_string() const override
    {
      std::size_t estimated_size = _method.size() + _target.size() + 64;
      for (const auto &[key, value] : _headers)
        estimated_size += key.size() + value.size() + 4; // ": " + "\r\n"

      std::string result;
      result.reserve(estimated_size);

      result += std::format("{} {} {} {} {} {}\r\n",_method, _target, _version,
      static_cast<std::uint8_t>(_checksum_type),_checksum_value, _content_length);

      if (!_user_agent.empty())
        result += std::format("User-Agent: {}\r\n", _user_agent);
      auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_timestamp.time_since_epoch()).count();
      result += std::format("Timestamp: {}\r\n", timestamp_ms);

      _serialize_headers_to_string(result);
      result += "\r\n";
      return result;
    }
    /**
     * @brief 从字符串反序列化
     * @param data 字符串数据
     * @return 是否成功
     */
    bool from_string(std::string_view data) override;

    /**
     * @brief 转换为JSON
     * @return JSON对象
     */
    json to_json() const override
    {
      json json_object = protocol_header::to_json();
      json_object.set("method", _method);
      json_object.set("target", _target);
      json_object.set("user_agent", _user_agent);

      auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_timestamp.time_since_epoch()).count();
      json_object.set("timestamp", timestamp_ms);

      return json_object;
    }
    /**
     * @brief 从`JSON`反序列化
     * @param json_object `JSON`对象
     * @return 是否成功
     */
    bool from_json(const json &json_object) override
    {
      if (!protocol_header::from_json(json_object))
        return false;

      try
      {
        _method = json_object.get<std::string>("method", "");
        _target = json_object.get<std::string>("target", "");
        _user_agent = json_object.get<std::string>("user_agent", "");
        auto timestamp_ms = json_object.get<std::int64_t>("timestamp", 0);
        _timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(timestamp_ms));
        return true;
      }
      catch (...)
      {
        return false;
      }
    }
  }; // end class request_header

  /**
   * @brief `TCP`协议响应头类
   * @details 实现`TCP`协议的响应头
   */
  class response_header : public auxiliary::protocol_header
  {
  private:
    std::string _server;                              // 服务器信息
    std::uint16_t _status_code = 200;                 // 状态码
    std::string _status_message = "OK";               // 状态消息
    std::chrono::system_clock::time_point _timestamp; // 时间戳
  private:
    /**
     * @brief 序列化头部字段为字符串
     * @param out 输出字符串
     */
    void _serialize_headers_to_string(std::string &out) const
    {
      std::vector<std::string> keys;
      keys.reserve(_headers.size());
      for (const auto &[key, value] : _headers)
        keys.push_back(key);
      std::sort(keys.begin(), keys.end());
      for (const std::string &key : keys)
      {
        const std::string &value = _headers.at(key);
        out.append(key);
        out.append(": ");
        out.append(value);
        out.append("\r\n");
      }
    }
  public:
    response_header()
    {
      _timestamp = std::chrono::system_clock::now();
      _protocol_type = auxiliary::protocol_type::CUSTOM_TCP;
    }
    std::uint16_t get_status_code() const noexcept { return _status_code; }
    void set_status_code(std::uint16_t code) { _status_code = code; }

    const std::string &get_status_message() const noexcept { return _status_message; }
    void set_status_message(const std::string &message) { _status_message = message; }

    const std::string &get_server() const noexcept { return _server; }
    void set_server(const std::string &server) { _server = server; }

    const std::chrono::system_clock::time_point &get_timestamp() const noexcept { return _timestamp; }
    void set_timestamp(const std::chrono::system_clock::time_point &timestamp) { _timestamp = timestamp; }

    std::string to_string() const override
    {
      std::size_t estimated_size = _status_message.size() + 64;
      for (const auto &[key, value] : _headers)
        estimated_size += key.size() + value.size() + 4;
      std::string result;
      result.reserve(estimated_size);
      result += std::format("{} {} {} {} {} {}\r\n",_version, _status_code, _status_message,
      static_cast<std::uint8_t>(_checksum_type),_checksum_value, _content_length);
      if (!_server.empty())
        result += std::format("Server: {}\r\n", _server);
      auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_timestamp.time_since_epoch()).count();
      result += std::format("Timestamp: {}\r\n", timestamp_ms);
      _serialize_headers_to_string(result);
      result += "\r\n";
      return result;
    }
    bool from_string(std::string_view data) override;
    /**
     * @brief 转换为JSON
     * @return JSON对象
     */
    json to_json() const override
    {
      json json_object = protocol_header::to_json();
      json_object.set("status_code", _status_code);
      json_object.set("status_message", _status_message);
      json_object.set("server", _server);
      auto timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_timestamp.time_since_epoch()).count();
      json_object.set("timestamp", timestamp_ms);
      return json_object;
    }
    bool from_json(const json &json_object) override
    {
      if (!protocol_header::from_json(json_object))
        return false;
      try
      {
        _status_code = json_object.get<std::uint16_t>("status_code", 200);
        _status_message = json_object.get<std::string>("status_message", "OK");
        _server = json_object.get<std::string>("server", "");
        auto timestamp_ms = json_object.get<std::int64_t>("timestamp", 0);
        _timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(timestamp_ms));
        return true;
      }
      catch (...)
      {
        return false;
      }
    }
  }; // end class response_header

  /**
   * @brief `TCP`协议请求类
   * @details 实现`TCP`协议的请求
   * @warning 使用该类需检查请求头是否符合 `header_constraint` 约束
   * @note 该类默认使用`request_header`作为请求头类型
   */
  template <header_constraint header_t = request_header>
  class request 
  {
  private:
    header_t _header;                       // 请求头
    std::string _message;                   // 请求体
    mutable std::string _cached_full;       // 缓存的完整请求字符串
    mutable bool _full_cache_valid = false; // 完整缓存是否有效
  private:
    /**
     * @brief 清除缓存
     */
    void _invalidate_cache() const noexcept
    {
      _full_cache_valid = false;
    } 
  public:
    request() = default;

    explicit request(const header_t &header) : _header(header) {}

    request(const header_t &header, const std::string &body)
    : _header(header), _message(body) {}

    request(const header_t &header, std::string &&body)
    : _header(header), _message(std::move(body)) {}

    // 拷贝和移动语义
    request(const request &other)
    : _header(other._header), _message(other._message) {}

    request(request &&other) noexcept
    : _header(std::move(other._header)), _message(std::move(other._message)), 
    _cached_full(std::move(other._cached_full)), _full_cache_valid(other._full_cache_valid)
    {
      other._full_cache_valid = false;
    }

    request &operator=(const request &other)
    {
      if (this != &other)
      {
        _header = other._header;
        _message = other._message;
        _invalidate_cache();
      }
      return *this;
    }

    request &operator=(request &&other) noexcept
    {
      if (this != &other)
      {
        _header = std::move(other._header);
        _message = std::move(other._message);
        _cached_full = std::move(other._cached_full);
        _full_cache_valid = other._full_cache_valid;
        other._full_cache_valid = false;
      }
      return *this;
    }
    const header_t &header() const noexcept { return _header; }
    header_t &header() noexcept
    {
      _invalidate_cache();
      return _header;
    }

    const std::string &body() const noexcept { return _message; }

    void set_message(const std::string &body)
    {
      _message = body;
      _invalidate_cache();
    }

    void set_message(std::string &&body)
    {
      _message = std::move(body);
      _invalidate_cache();
    }
    /**
     * @brief 序列化为完整的请求字符串
     * @return 完整的请求字符串
     */
    const std::string &to_string() const
    {
      if (!_full_cache_valid)
      { // `calculate_and_set_checksum` 作用： 计算消息体校验码并设置校验码
        const_cast<header_t &>(_header).calculate_and_set_checksum(_message);

        _cached_full = _header.to_string() + _message;
        _full_cache_valid = true;
      }
      return _cached_full;
    }
    /**
     * @brief 从字符串反序列化
     * @param data 完整的请求字符串
     * @return 是否成功
     */
    bool from_string(std::string_view data)
    {
      std::size_t header_end = data.find("\r\n\r\n");
      if (header_end == std::string_view::npos)
        return false;

      std::string_view header_data = data.substr(0, header_end + 2); // 包含最后的\r\n
      if (!_header.from_string(header_data))
        return false;
      // 提取body
      std::size_t body_start = header_end + 4; // 跳过 "\r\n\r\n"
      if (body_start < data.size())
        _message = std::string(data.substr(body_start));
      else
        _message.clear();
      _invalidate_cache();
      return _header.verify_integrity(_message);
    }
    /**
     * @brief 转换为JSON
     * @return JSON对象
     */
    json to_json() const
    {
      json json_object;
      json_object.set("header", _header.to_json().to_string());
      json_object.set("body", _message);
      return json_object;
    }
    /**
     * @brief 从`JSON`反序列化
     * @param json_object JSON对象
     * @return 是否成功
     */
    bool from_json(const json &json_object)
    {
      try
      {
        std::string header_json_str = json_object.get<std::string>("header", "");
        if (!header_json_str.empty())
        {
          json header_json(header_json_str);
          if (!_header.from_json(header_json))
            return false;
        }
        _message = json_object.get<std::string>("body", "");
        _invalidate_cache();
        return true;
      }
      catch (...)
      {
        return false;
      }
    }

    /**
     * @brief 验证请求完整性
     * @return 验证是否通过
     */
    bool verify_integrity() const
    {
      return _header.verify_integrity(_message);
    }
    /**
     * @brief 获取请求大小（字节）
     * @return 请求大小
     */
    std::size_t size() const
    {
      return to_string().size();
    }
    /**
     * @brief 检查是否为空请求
     * @return 是否为空
     */
    bool empty() const noexcept
    {
      return _message.empty();
    }

    bool operator==(const request &other) const noexcept
    {
      return _header.to_string() == other._header.to_string() && _message == other._message;
    }

    bool operator!=(const request &other) const noexcept
    {
      return !(*this == other);
    }
  }; // end class request

  /**
   * @brief `TCP`协议响应头类
   * @details 实现`TCP`协议的响应头
   * @warning 使用该类需检查响应头是否符合 `header_constraint` 约束
   * @note 该类默认使用`response_header`作为响应头类型
   */
  template<header_constraint header_t = response_header>
  class response
  {
  private:
    header_t _header;                       // 响应头
    std::string _message;                   // 响应体
    mutable std::string _cached_full;       // 缓存的完整响应字符串
    mutable bool _full_cache_valid = false; // 完整缓存是否有效

  private:
    /**
     * @brief 清除缓存
     */
    void _invalidate_cache() const noexcept
    {
      _full_cache_valid = false;
    }

  public:
    response() = default;

    explicit response(const header_t &header) : _header(header) {}

    response(const header_t &header, const std::string &body)
    : _header(header), _message(body) {}

    response(const header_t &header, std::string &&body)
    : _header(header), _message(std::move(body)) {}

    // 便利构造函数（用于快速创建响应）
    response(std::uint16_t status_code, const std::string &status_message, const std::string &body = "")
    : _message(body)
    {
      if constexpr (std::is_same_v<header_t, response_header>)
      {
        _header.set_status_code(status_code);
        _header.set_status_message(status_message);
      }
    }
    response(const response &other)
    : _header(other._header), _message(other._message) {}

    response(response &&other) noexcept
    : _header(std::move(other._header)), _message(std::move(other._message)), _cached_full(std::move(other._cached_full)), _full_cache_valid(other._full_cache_valid)
    {
      other._full_cache_valid = false;
    }

    response &operator=(const response &other)
    {
      if (this != &other)
      {
        _header = other._header;
        _message = other._message;
        _invalidate_cache();
      }
      return *this;
    }

    response &operator=(response &&other) noexcept
    {
      if (this != &other)
      {
        _header = std::move(other._header);
        _message = std::move(other._message);
        _cached_full = std::move(other._cached_full);
        _full_cache_valid = other._full_cache_valid;
        other._full_cache_valid = false;
      }
      return *this;
    }
    const header_t &header() const noexcept { return _header; }

    header_t &header() noexcept
    {
      _invalidate_cache();
      return _header;
    }

    const std::string &body() const noexcept { return _message; }

    void set_message(const std::string &body)
    {
      _message = body;
      _invalidate_cache();
    }

    void set_message(std::string &&body)
    {
      _message = std::move(body);
      _invalidate_cache();
    }

    // 便利方法（仅适用于 response_header ） 如果类型不符合 则忽略函数

    template <typename current_header_t = header_t>
    std::enable_if_t<std::is_same_v<current_header_t, response_header>, std::uint16_t>
    get_status_code() const noexcept
    {
      return _header.get_status_code();
    }

    template <typename current_header_t = header_t>
    std::enable_if_t<std::is_same_v<current_header_t, response_header>, void>
    set_status_code(std::uint16_t code)
    {
      _header.set_status_code(code);
      _invalidate_cache();
    }

    template <typename current_header_t = header_t>
    std::enable_if_t<std::is_same_v<current_header_t, response_header>, const std::string &>
    get_status_message() const noexcept
    {
      return _header.get_status_message();
    }

    template <typename current_header_t = header_t>
    std::enable_if_t<std::is_same_v<current_header_t, response_header>, void>
    set_status_message(const std::string &message)
    {
      _header.set_status_message(message);
      _invalidate_cache();
    }

    const std::string &to_string() const
    {
      if (!_full_cache_valid)
      {
        // 先计算并设置校验值
        const_cast<header_t &>(_header).calculate_and_set_checksum(_message);

        _cached_full = _header.to_string() + _message;
        _full_cache_valid = true;
      }
      return _cached_full;
    }

    /**
     * @brief 从字符串反序列化
     * @param data 完整的响应字符串
     * @return 是否成功
     */
    bool from_string(std::string_view data)
    {
      // 查找头部结束标志
      std::size_t header_end = data.find("\r\n\r\n");
      if (header_end == std::string_view::npos)
        return false;

      // 解析头部
      std::string_view header_data = data.substr(0, header_end + 2); // 包含最后的\r\n
      if (!_header.from_string(header_data))
        return false;

      // 提取body
      std::size_t body_start = header_end + 4; // 跳过 "\r\n\r\n"
      if (body_start < data.size())
        _message = std::string(data.substr(body_start));
      else
        _message.clear();
      _invalidate_cache();
      // 验证数据完整性
      return _header.verify_integrity(_message);
    }

    json to_json() const
    {
      json json_object;
      json_object.set("header", _header.to_json().to_string());
      json_object.set("body", _message);
      return json_object;
    }
    /**
     * @brief 从JSON反序列化
     * @param json_object JSON对象
     * @return 是否成功
     */
    bool from_json(const json &json_object)
    {
      try
      {
        std::string header_json_str = json_object.get<std::string>("header", "");
        if (!header_json_str.empty())
        {
          json header_json(header_json_str);
          if (!_header.from_json(header_json))
            return false;
        }
        _message = json_object.get<std::string>("body", "");
        _invalidate_cache();
        return true;
      }
      catch (...)
      {
        return false;
      }
    }

    /**
     * @brief 验证响应完整性
     * @return 验证是否通过
     */
    bool verify_integrity() const
    {
      return _header.verify_integrity(_message);
    }
    /**
     * @brief 获取响应大小（字节）
     * @return 响应大小
     */
    std::uint64_t size() const
    {
      return to_string().size();
    }
    /**
     * @brief 检查是否为空响应
     * @return 是否为空
     */
    bool empty() const noexcept
    {
      return _message.empty();
    }

    bool operator==(const response &other) const noexcept
    {
      return _header.to_string() == other._header.to_string() && _message == other._message;
    }

    bool operator!=(const response &other) const noexcept
    {
      return !(*this == other);
    }

    static response create_success(const std::string &body = "", const std::string &server = "")
    {
      response resp(200, "OK", body);
      if constexpr (std::is_same_v<header_t, response_header>)
      {
        if (!server.empty())
          resp._header.set_server(server);
      }
      return resp;
    }
    static response create_error(std::uint16_t code, const std::string &message, const std::string &body = "")
    {
      return response(code, message, body);
    }

    static response create_not_found(const std::string &body = "not found")
    {
      return response(404, "not found", body);
    }

    static response create_internal_error(const std::string &body = "internal server error")
    {
      return response(500, "internal server error", body);
    }
  }; // end class response
  namespace conversion
  {

  } // end namespace conversion
} // end namespace protocol


bool protocol::request_header::from_string(std::string_view data)
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
    _method = std::string(parts[0]);
    _target = std::string(parts[1]);
    std::uint8_t ctype_val;
    if (!parse(parts[2], _version) || !parse(parts[3], ctype_val) ||!parse(parts[4], _checksum_value) 
        || !parse(parts[5], _content_length))
      return false;
    _checksum_type = static_cast<auxiliary::checksum_type>(ctype_val);
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
        if (key == "User-Agent")
          _user_agent = val;
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
  { // 空白修剪
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
