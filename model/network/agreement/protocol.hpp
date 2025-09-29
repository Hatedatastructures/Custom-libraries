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
#include "encryption.hpp"
#include "auxiliary.hpp"

namespace protocol
{
  // 协议头约束
  template <typename request_header_t>
  concept header_constraint = requires(request_header_t header,std::string_view data,const json& json_object,std::string_view content) 
  {
    { header.to_string() } -> std::same_as<std::string>;
    { header.from_string(data) }  -> std::same_as<bool>;
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

  }; // end class response_header

  /**
   * @brief `TCP`协议请求头类
   * @details 实现`TCP`协议的请求头
   * @warning 使用该类需检查请求头是否符合 `header_constraint` 约束
   * @note 该类默认使用`request_header`作为请求头类型
   */
  template <header_constraint header_t = request_header>
  class request 
  {

  };
} // end namespace protocol

bool protocol::request_header::from_string(std::string_view data)
{
  if (data.empty())
    return false;
  _headers.clear();

  auto parse_num = [](std::string_view sv, auto &out)
  {
    return std::from_chars(sv.data(), sv.data() + sv.size(), out).ec == std::errc{};
  };

  auto trim = [](std::string_view &sv)
  {
    while (!sv.empty() && std::isspace(sv.front()))
      sv.remove_prefix(1);
    while (!sv.empty() && std::isspace(sv.back()))
      sv.remove_suffix(1);
  };

  std::size_t pos = 0, data_size = data.size();
  std::size_t line_end = data.find("\r\n", pos);
  if (line_end == std::string_view::npos)
    return false;

  std::string_view req_line = data.substr(pos, line_end - pos);
  std::vector<std::string_view> parts;
  for (std::size_t i = 0, start = 0; i <= req_line.size(); ++i)
  {
    if (i == req_line.size() || req_line[i] == ' ')
    {
      if (i > start)
        parts.push_back(req_line.substr(start, i - start));
      start = i + 1;
    }
  }
  if (parts.size() < 6)
    return false;

  _method = std::string(parts[0]);
  _target = std::string(parts[1]);
  if (!parse_num(parts[2], _version))
    return false;

  std::uint8_t checksum_type_val;
  if (!parse_num(parts[3], checksum_type_val))
    return false;
  _checksum_type = static_cast<checksum_type>(checksum_type_val);

  if (!parse_num(parts[4], _checksum_value))
    return false;
  if (!parse_num(parts[5], _content_length))
    return false;

  for (pos = line_end + 2; pos < data_size; pos = line_end + 2)
  {
    if ((line_end = data.find("\r\n", pos)) == std::string_view::npos)
      break;
    std::string_view line = data.substr(pos, line_end - pos);
    if (line.empty())
      break;

    std::size_t colon_pos = line.find(':');
    if (colon_pos == std::string_view::npos)
      continue;

    std::string_view key = line.substr(0, colon_pos);
    std::string_view val = line.substr(colon_pos + 1);
    trim(key);
    trim(val);

    std::string key_str(key), val_str(val);
    if (key_str == "User-Agent")
    {
      _user_agent = val_str;
    }
    else if (key_str == "Timestamp")
    {
      std::int64_t ts;
      if (parse_num(val_str, ts))
      {
        _timestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(ts));
      }
    }
    else
    {
      _headers[key_str] = val_str;
    }
  }
  return true;
}