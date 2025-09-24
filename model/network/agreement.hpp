#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <charconv>
#include <cctype>
#include <format> 
#include <cstdint>
#include <utility>
#include <json/json.h>
#include <boost/json.hpp>
#include "encryption.hpp"
#include "processing.hpp"

namespace agreement
{
  /**
   * @brief 请求头
   * @details 包含请求方法、校验码、头部字段长度和头部字段容器
   * @note 通过校验码保证数据完整性
   */
  class request_header
  {
  private:
    void assignment_logic(const request_header &other)
    {
      method = other.method;
      verification_code = other.verification_code;
      headers_string_len = other.headers_string_len;
      headers = other.headers;
    }
    void assignment_logic(request_header &&other)
    {
      method = std::move(other.method);
      verification_code = other.verification_code;
      headers_string_len = other.headers_string_len;
      headers = std::move(other.headers);
    }

  protected:
    /**
     * @brief 序列化 headers 的实现
     * @warning 必须保证 headers 中的 key 是有序的，否则校验码计算会不一致
     */
    virtual void serialize_headers_to_string(std::string &out) const
    {
      // 先把哈希表的key映射到vector中 保证每次序列化顺序一致,保证校验码计算的准确性
      std::vector<const std::string *> keys;
      keys.reserve(headers.size());
      for (const auto &kv : headers)
        keys.push_back(&kv.first);
      std::sort(keys.begin(), keys.end(), [](const std::string *a, const std::string *b){ return *a < *b; });

      // 把键值追加到string中
      for (const std::string *key_ptr : keys)
      {
        const std::string &key = *key_ptr;
        const std::string &value = headers.at(key);
        out.append(key);
        out.append(": ");
        out.append(value);
        out.append("\r\n");
      }
    }

  public:
    std::string method;                                   // 请求方法
    std::uint32_t verification_code = 0;                  // 校验码
    std::uint64_t headers_string_len = 0;                 // 头部字段字节长度（用于 `CRC`计算 ）
    std::unordered_map<std::string, std::string> headers; // 头部字段容器

    request_header() { headers.reserve(12); }

    request_header(const request_header &other) { assignment_logic(other); }
    request_header(request_header &&other) { assignment_logic(std::forward<request_header>(other)); }

    request_header &operator=(const request_header &other)
    {
      if (&other != this)
        assignment_logic(other);
      return *this;
    }
    request_header &operator=(request_header &&other)
    {
      if (&other != this)
        assignment_logic(std::forward<request_header>(other));
      return *this;
    }

    /**
     * @brief 将请求头序列化为字符串（方法行 + headers + 空行）
     * @note  `std::format` 风格，但 `headers` 使用 `append` 减少中间分配
     */
    std::string to_string() const
    {
      // 估算输出容量，尽量减少 reallocation
      std::uint64_t estimate_size = static_cast<std::uint64_t>(method.size()) + 32 + static_cast<std::uint64_t>(headers.size()) * 32;
      std::string out;
      out.reserve(static_cast<std::size_t>(estimate_size + 16));

      out += std::format("{} {}\r\n", method, verification_code);
      serialize_headers_to_string(out);
      out.append("\r\n");
      return out;
    }

    std::uint32_t calculation()
    {
      verification_code = encryption::CyclicRedundancyCheck32(to_string(), headers_string_len);
      return verification_code;
    }
    bool verification() const
    {
      return verification_code == encryption::CyclicRedundancyCheck32(to_string(), headers_string_len);
    }

    // 兼容接口：接受 std::string 引用（内部转为 string_view 处理）
    bool from_string(const std::string &request_string)
    {
      std::string_view header_view(request_string.data(), request_string.size());
      return from_string(header_view);
    }

    /**
     * @brief 高效解析接口：接受 header 字符串片段（不包含 `body`），使用 `std::string_view` 零拷贝解析
     * @param header_view 只包含头部（直到 `\r\n\r\n` 前）的一段 `std::string_view`
     * @return 解析成功返回 true，格式错误或解析失败返回 false
     * @note 相比于json在网络中的解析和序列化，这个方法比之快3以上
     */
    bool from_string(std::string_view header_view)
    {
      if (header_view.empty())
        return false;

      headers.clear();
      headers.reserve(12); 

      const char *raw_data = header_view.data();
      std::uint64_t header_total_length = static_cast<std::uint64_t>(header_view.size());
      std::uint64_t parse_position = 0;

      // 查找第一行结束符 '\n'
      std::uint64_t first_line_newline_pos = static_cast<std::uint64_t>(header_view.find('\n', parse_position));
      if (first_line_newline_pos == std::string_view::npos)
        return false;

      // 计算第一行实际结束（去掉可能的 '\r'）
      std::uint64_t first_line_end_pos = first_line_newline_pos;
      if (first_line_end_pos > 0 && raw_data[first_line_end_pos - 1] == '\r')
        --first_line_end_pos;

      // 提取第一行内容并解析 method 与 verification_code
      std::string_view first_line_view(raw_data + 0, static_cast<std::size_t>(first_line_end_pos));

      // 解析 method（第一个 token）
      std::uint64_t idx = 0;
      while (idx < first_line_view.size() && first_line_view[idx] != ' ' && first_line_view[idx] != '\t')
        ++idx;
      if (idx == 0 || idx >= first_line_view.size())
        return false;
      method.assign(first_line_view.data(), static_cast<std::size_t>(idx));

      // 跳过空白到 verification_code 开始
      std::uint64_t code_start_index = idx;
      while (code_start_index < first_line_view.size() && (first_line_view[code_start_index] == ' ' || first_line_view[code_start_index] == '\t'))
        ++code_start_index;
      if (code_start_index >= first_line_view.size())
        return false;

      // 找到 verification_code 结束
      std::uint64_t code_end_index = code_start_index;
      while (code_end_index < first_line_view.size() && first_line_view[code_end_index] != ' ' && first_line_view[code_end_index] != '\t')
        ++code_end_index;
      std::string_view code_subview(first_line_view.data() + code_start_index, static_cast<std::size_t>(code_end_index - code_start_index));

      // 使用 from_chars 解析整数
      {
        auto parse_result = std::from_chars(code_subview.data(), code_subview.data() + code_subview.size(), verification_code);
        if (parse_result.ec != std::errc() || parse_result.ptr != code_subview.data() + code_subview.size())
          return false;
      }

      parse_position = first_line_newline_pos + 1;
      if (parse_position > header_total_length)
        return false;

      while (parse_position < header_total_length)
      {
        // 查找本行结尾 '\n'
        std::uint64_t line_newline_pos = static_cast<std::uint64_t>(header_view.find('\n', parse_position));
        std::uint64_t line_end_pos = (line_newline_pos == std::string_view::npos) ? header_total_length : line_newline_pos;
        std::uint64_t line_start_pos = parse_position;
        std::uint64_t raw_line_length = (line_end_pos > line_start_pos) ? (line_end_pos - line_start_pos) : 0;

        // 去掉尾部可能的 '\r'
        if (raw_line_length > 0 && raw_data[line_start_pos + raw_line_length - 1] == '\r')
          --raw_line_length;

        if (raw_line_length == 0)
          break;

        std::string_view line_view(raw_data + line_start_pos, static_cast<std::size_t>(raw_line_length));
        std::uint64_t colon_pos_in_line = static_cast<std::uint64_t>(line_view.find(':'));
        if (colon_pos_in_line == std::string_view::npos)
          return false;

        std::string_view raw_key_view = line_view.substr(0, static_cast<std::size_t>(colon_pos_in_line));
        raw_key_view = agreement_detail_processing::rtrim_right(raw_key_view);
        if (raw_key_view.empty())
          return false;

        std::string_view raw_value_view = line_view.substr(static_cast<std::size_t>(colon_pos_in_line + 1));
        raw_value_view = agreement_detail_processing::trim_both_ends(raw_value_view);

        std::string key_string(raw_key_view.data(), raw_key_view.size());
        std::string value_string(raw_value_view.data(), raw_value_view.size());
        headers.emplace(std::move(key_string), std::move(value_string));

        if (line_newline_pos == std::string_view::npos)
        {
          parse_position = header_total_length;
          break;
        }
        parse_position = line_newline_pos + 1;
      }
      headers_string_len = static_cast<std::uint64_t>(to_string().size());
      return true;
    }

    std::string &operator[](const std::string &key)     { return headers[key];      }
    const std::string &at(const std::string &key) const { return headers.at(key); }
  }; // request_header
  /**
   * @brief 请求类
   * @details 包含请求头和请求体
   * @note 模板参数 request_header_t 为请求头类型，默认使用 request_header
   */
  template <class request_header_t = request_header>
  class request
  {
  public:
    request_header_t information;
    std::string streaming_message_body;

    request() = default;

    request(const request_header_t &info, const std::string &body)
    : information(info), streaming_message_body(body) {}

    std::string to_string() const
    {
      std::string header_str = information.to_string();
      std::string out;
      out.reserve(header_str.size() + streaming_message_body.size());
      out.append(header_str);
      out.append(streaming_message_body);
      return out;
    }

    bool from_string(const std::string &request_string)
    {
      std::size_t header_boundary_pos = request_string.find("\r\n\r\n");
      if (header_boundary_pos == std::string::npos)
        return false;
      std::string_view header_view(request_string.data(), header_boundary_pos);
      if (!information.from_string(header_view))
        return false;

      if (!information.verification())
        return false;

      streaming_message_body.assign(request_string.data() + header_boundary_pos + 4, request_string.size() - (header_boundary_pos + 4));
      return true;
    }
  }; // request

  /**
   * @brief 响应头类
   * @details 包含响应状态码、状态消息、校验码、头部字段长度和头部字段容器
   * @note 通过校验码保证数据完整性
   */
  class response_header
  {
  protected:
    virtual void serialize_headers_to_string(std::string &out) const
    {
      std::vector<const std::string *> keys;
      keys.reserve(headers.size());
      for (const auto &kv : headers)
        keys.push_back(&kv.first);
      std::sort(keys.begin(), keys.end(), [](const std::string *a, const std::string *b){ return *a < *b; });
      for (const std::string *kp : keys)
      {
        const std::string &key = *kp;
        const std::string &value = headers.at(key);
        out.append(key);
        out.append(": ");
        out.append(value);
        out.append("\r\n");
      }
    }

  public:
    std::string status_msg;
    std::uint16_t status_code = 200;
    std::uint32_t verification_code = 0;
    std::uint32_t headers_string_len = 0;
    std::unordered_map<std::string, std::string> headers;

    response_header() { headers.reserve(12); }

    // 序列化（状态行 + headers + 空行）
    std::string to_string() const
    {
      std::uint64_t estimate_size = 32 + static_cast<std::uint64_t>(status_msg.size()) + static_cast<std::uint64_t>(headers.size()) * 32;
      std::string out;
      out.reserve(static_cast<std::size_t>(estimate_size));

      out += std::format("{} {} {}\r\n", status_code, status_msg, verification_code);
      serialize_headers_to_string(out);
      out.append("\r\n");
      return out;
    }

    bool from_string(const std::string &response_string)
    {
      std::string_view header_view(response_string.data(), response_string.size());
      return from_string(header_view);
    }

    bool from_string(std::string_view header_view)
    {
      if (header_view.empty())
        return false;
      headers.clear();
      headers.reserve(12);

      const char *raw_data = header_view.data();
      std::uint64_t header_total_length = static_cast<std::uint64_t>(header_view.size());
      std::uint64_t parse_position = 0;

      // 解析第一行（状态行）
      std::uint64_t first_line_newline_pos = static_cast<std::uint64_t>(header_view.find('\n', parse_position));
      if (first_line_newline_pos == std::string_view::npos)
        return false;

      std::uint64_t first_line_end_pos = first_line_newline_pos;
      if (first_line_end_pos > 0 && raw_data[first_line_end_pos - 1] == '\r')
        --first_line_end_pos;

      std::string_view first_line_view(raw_data + 0, static_cast<std::size_t>(first_line_end_pos));

      std::uint64_t idx = 0;
      while (idx < first_line_view.size() && !std::isspace(static_cast<unsigned char>(first_line_view[idx])))
        ++idx;
      if (idx == 0 || idx >= first_line_view.size())
        return false;

      unsigned int tmp_status_code = 0;
      {
        auto r = std::from_chars(first_line_view.data(), first_line_view.data() + idx, tmp_status_code);
        if (r.ec != std::errc())
          return false;
      }
      status_code = static_cast<std::uint16_t>(tmp_status_code);

      // 解析 status_msg（下一个 token）
      std::uint64_t msg_start = idx;
      while (msg_start < first_line_view.size() && std::isspace(static_cast<unsigned char>(first_line_view[msg_start])))
        ++msg_start;
      if (msg_start >= first_line_view.size())
        return false;

      std::uint64_t msg_end = msg_start;
      while (msg_end < first_line_view.size() && !std::isspace(static_cast<unsigned char>(first_line_view[msg_end])))
        ++msg_end;
      status_msg.assign(first_line_view.data() + msg_start, static_cast<std::size_t>(msg_end - msg_start));

      // 解析 verification_code（剩余部分）
      std::uint64_t code_start = msg_end;
      while (code_start < first_line_view.size() && std::isspace(static_cast<unsigned char>(first_line_view[code_start])))
        ++code_start;
      if (code_start >= first_line_view.size())
        return false;
      {
        auto r = std::from_chars(first_line_view.data() + code_start,first_line_view.data() + first_line_view.size(),
        verification_code);
        if (r.ec != std::errc())
          return false;
      }

      // 移动到 headers 开始
      parse_position = first_line_newline_pos + 1;

      // 解析 headers 行（与 request_header 类似）
      while (parse_position < header_total_length)
      {
        std::uint64_t line_newline_pos = static_cast<std::uint64_t>(header_view.find('\n', parse_position));
        std::uint64_t line_end_pos = (line_newline_pos == std::string_view::npos) ? header_total_length : line_newline_pos;
        std::uint64_t line_start_pos = parse_position;
        std::uint64_t line_length = (line_end_pos > line_start_pos) ? (line_end_pos - line_start_pos) : 0;

        if (line_length > 0 && raw_data[line_start_pos + line_length - 1] == '\r')
          --line_length;
        if (line_length == 0)
          break;

        std::string_view line_view(raw_data + line_start_pos, static_cast<std::size_t>(line_length));
        std::uint64_t colon_pos = static_cast<std::uint64_t>(line_view.find(':'));
        if (colon_pos == std::string_view::npos)
          return false;

        std::string_view key_view = line_view.substr(0, static_cast<std::size_t>(colon_pos));
        key_view = agreement_detail_processing::rtrim_right(key_view);
        if (key_view.empty())
          return false;

        std::string_view value_view = line_view.substr(static_cast<std::size_t>(colon_pos + 1));
        value_view = agreement_detail_processing::trim_both_ends(value_view);

        headers.emplace(std::string(key_view.data(), key_view.size()),std::string(value_view.data(), value_view.size()));

        if (line_newline_pos == std::string_view::npos)
          break;
        parse_position = line_newline_pos + 1;
      }
      headers_string_len = static_cast<std::uint32_t>(to_string().size());
      return true;
    }
  }; // response_header
  /**
   * @brief 响应类
   * @details 包含响应头和响应体
   * @note 模板参数 response_header_t 为响应头类型，默认使用 response_header
   */
  template <class response_header_t = response_header>
  class response
  {
  public:
    response_header_t information;
    std::string streaming_message_body;

    std::string to_string() const
    {
      std::string header_str = information.to_string();
      std::string out;
      out.reserve(header_str.size() + streaming_message_body.size());
      out.append(header_str);
      out.append(streaming_message_body);
      return out;
    }

    bool from_string(const std::string &data)
    {
      std::size_t header_end_pos = data.find("\r\n\r\n");
      if (header_end_pos == std::string::npos)
        return false;
      std::string_view header_view(data.data(), header_end_pos);
      if (!information.from_string(header_view))
        return false;
      streaming_message_body.assign(data.data() + header_end_pos + 4, data.size() - (header_end_pos + 4));
      return true;
    }
  }; // response

  /**
   * @brief 将 `request_header` 转换为 `JSON` 值
   * @param h request_header 实例
   * @return boost::json::value 转换后的 JSON 值
   */
  inline boost::json::value to_json(const request_header &h)
  {
    boost::json::object obj;
    obj["method"] = h.method;
    obj["verification_code"] = static_cast<uint64_t>(h.verification_code);
    obj["headers_string_len"] = static_cast<uint64_t>(h.headers_string_len);
    obj["headers"] = agreement_detail_processing::map_to_json_object(h.headers);
    return boost::json::value(std::move(obj));
  }

  inline boost::json::value to_json(const response_header &h)
  {
    boost::json::object obj;
    obj["status_msg"] = h.status_msg;
    obj["status_code"] = static_cast<uint64_t>(h.status_code);
    obj["verification_code"] = static_cast<uint64_t>(h.verification_code);
    obj["headers_string_len"] = static_cast<uint64_t>(h.headers_string_len);
    obj["headers"] = agreement_detail_processing::map_to_json_object(h.headers);
    return boost::json::value(std::move(obj));
  }
  template <typename header_t>
  inline boost::json::value to_json(const request<header_t> &r)
  {
    boost::json::object obj;
    obj["information"] = to_json(r.information); 
    obj["streaming_message_body"] = r.streaming_message_body;
    return boost::json::value(std::move(obj));
  }
  template <typename header_t>
  inline boost::json::value to_json(const response<header_t> &r)
  {
    boost::json::object obj;
    obj["information"] = to_json(r.information); 
    obj["streaming_message_body"] = r.streaming_message_body;
    return boost::json::value(std::move(obj));
  }

  inline bool from_json(const boost::json::value &v, request_header &out, std::string *err = nullptr)
  {
    try
    {
      if (!agreement_detail_processing::validate_json_object(v, "request_header", err))
        return false;
      
      const auto &o = v.as_object();
      agreement_detail_processing::string_from_json(o, "method", out.method);
      agreement_detail_processing::uint32_from_json(o, "verification_code", out.verification_code);
      agreement_detail_processing::uint64_from_json(o, "headers_string_len", out.headers_string_len);
      agreement_detail_processing::headers_from_json(o, out.headers);
      
      return true;
    }
    catch (const std::exception &ex)
    {
      if (err)
        *err = ex.what();
      return false;
    }
  }
  inline bool from_json(const boost::json::value &v, response_header &out, std::string *err = nullptr)
  {
    try
    {
      if (!agreement_detail_processing::validate_json_object(v, "response_header", err))
        return false;
      
      const auto &o = v.as_object();
      agreement_detail_processing::string_from_json(o, "status_msg", out.status_msg);
      agreement_detail_processing::uint16_from_json(o, "status_code", out.status_code);
      agreement_detail_processing::uint32_from_json(o, "verification_code", out.verification_code);
      agreement_detail_processing::uint32_from_json(o, "headers_string_len", out.headers_string_len);
      agreement_detail_processing::headers_from_json(o, out.headers);
      
      return true;
    }
    catch (const std::exception &ex)
    {
      if (err)
        *err = ex.what();
      return false;
    }
  }
  template <typename header_t>
  inline bool from_json(const boost::json::value &v, request<header_t> &out, std::string *err = nullptr)
  {
    try
    {
      if (!agreement_detail_processing::validate_json_object(v, "request", err))
        return false;
      const auto &o = v.as_object();
      if (!agreement_detail_processing::check_required_field(o, "information", "request", "information", err))
        return false;
      if (!from_json(o.at("information"), out.information, err))
        return false;
      agreement_detail_processing::string_from_json(o, "streaming_message_body", out.streaming_message_body);
      return true;
    }
    catch (const std::exception &ex)
    {
      if (err)
        *err = ex.what();
      return false;
    }
  }

  template <typename header_t>
  inline bool from_json(const boost::json::value &v, response<header_t> &out, std::string *err = nullptr)
  {
    try
    {
      if (!agreement_detail_processing::validate_json_object(v, "response", err))
        return false;
      const auto &o = v.as_object();
      if (!agreement_detail_processing::check_required_field(o, "information", "response", "information", err))
        return false;
      if (!from_json(o.at("information"), out.information, err))
        return false;
      agreement_detail_processing::string_from_json(o, "streaming_message_body", out.streaming_message_body);
      return true;
    }
    catch (const std::exception &ex)
    {
      if (err)
        *err = ex.what();
      return false;
    }
  }

} // namespace agreement
