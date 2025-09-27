#pragma once
#include <iostream>
#include <string_view>
#include <unordered_map>
#include <string>
#include <boost/json.hpp>

namespace agreement
{
  /**
   * @brief 协议模块处理字符串的操作
   */
  namespace agreement_detail_processing
  {
    /**
     * @brief 移除字符串首尾的空格和制表符
     * @param sv 输入的字符串视图
     * @return `std::string_view` 移除首尾空格和制表符后的字符串视图
     */
    inline std::string_view trim_both_ends(std::string_view sv)
    {
      std::uint64_t begin_index = 0;
      std::uint64_t end_index = static_cast<std::uint64_t>(sv.size());
      while (begin_index < end_index && (sv[begin_index] == ' ' || sv[begin_index] == '\t'))
        ++begin_index;
      while (end_index > begin_index && (sv[end_index - 1] == ' ' || sv[end_index - 1] == '\t'))
        --end_index;
      return sv.substr(begin_index, end_index - begin_index);
    }
    /**
     * @brief 移除字符串右侧的空格和制表符
     * @param sv 输入的字符串视图
     * @return `std::string_view` 移除右侧空格和制表符后的字符串视图
     */
    inline std::string_view rtrim_right(std::string_view sv)
    {
      std::uint64_t end_index = static_cast<std::uint64_t>(sv.size());
      std::uint64_t begin_index = 0;
      while (end_index > begin_index && (sv[end_index - 1] == ' ' || sv[end_index - 1] == '\t'))
        --end_index;
      return sv.substr(begin_index, end_index - begin_index);
    }

    // 将无符号整数追加到字符串，使用 std::to_chars（避免 sprintf 等调用）
    inline void append_uint_to_string(std::string &out, std::uint64_t value)
    {
      char buffer[32];
      auto res = std::to_chars(buffer, buffer + sizeof(buffer), value);
      out.append(buffer, static_cast<std::size_t>(res.ptr - buffer));
    }

    // 将 `std::unordered_map` 转换为 `boost::json::object`
    inline boost::json::object map_to_json_object(const std::unordered_map<std::string, std::string> &m)
    {
      boost::json::object o;
      for (auto const &kv : m)
        o.emplace(kv.first, kv.second);
      return o;
    }

    // 将 `boost::json::object` 转换为 `std::unordered_map`
    inline std::unordered_map<std::string, std::string> json_object_to_map(const boost::json::object &o)
    {
      std::unordered_map<std::string, std::string> m;
      m.reserve(o.size());
      for (auto const &kv : o)
      {
        if (kv.value().is_string())
          m.emplace(std::string(kv.key_c_str()), kv.value().as_string().c_str());
        else
          m.emplace(std::string(kv.key_c_str()), std::string{});
      }
      return m;
    }

    // 从`JSON`对象中获取字符串值
    inline bool string_from_json(const boost::json::object &obj, const std::string &key, std::string &out)
    {
      if (auto it = obj.if_contains(key); it && it->is_string())
      {
        out = it->as_string().c_str();
        return true;
      }
      return false;
    }

    // 从`JSON`对象中获取无符号32位整数值
    inline bool uint32_from_json(const boost::json::object &obj, const std::string &key, std::uint32_t &out)
    {
      if (auto it = obj.if_contains(key))
      {
        if (it->is_uint64())
        {
          out = static_cast<std::uint32_t>(it->as_uint64());
          return true;
        }
        else if (it->is_int64())
        {
          out = static_cast<std::uint32_t>(it->as_int64());
          return true;
        }
      }
      return false;
    }

    // 从`JSON`对象中获取无符号64位整数值
    inline bool uint64_from_json(const boost::json::object &obj, const std::string &key, std::uint64_t &out)
    {
      if (auto it = obj.if_contains(key))
      {
        if (it->is_uint64())
        {
          out = it->as_uint64();
          return true;
        }
        else if (it->is_int64())
        {
          out = static_cast<std::uint64_t>(it->as_int64());
          return true;
        }
      }
      return false;
    }

    // 从`JSON`对象中获取16位无符号整数值
    inline bool uint16_from_json(const boost::json::object &obj, const std::string &key, std::uint16_t &out)
    {
      if (auto it = obj.if_contains(key))
      {
        if (it->is_uint64())
        {
          out = static_cast<std::uint16_t>(it->as_uint64());
          return true;
        }
        else if (it->is_int64())
        {
          out = static_cast<std::uint16_t>(it->as_int64());
          return true;
        }
      }
      return false;
    }

    //  验证`JSON`值是否为对象类型
    inline bool validate_json_object(const boost::json::value &v, const std::string &type_name, std::string *err)
    {
      if (!v.is_object())
      {
        if (err)
          *err = type_name + ": json is not object";
        return false;
      }
      return true;
    }

    // 检查`JSON`对象中是否包含必需的字段
    inline bool check_required_field(const boost::json::object &obj, const std::string &key, 
    const std::string &type_name, const std::string &field_name, std::string *err)
    {
      if (!obj.if_contains(key))
      {
        if (err)
          *err = type_name + ": missing " + field_name;
        return false;
      }
      return true;
    }

    // 从`JSON`对象中提取headers映射
    inline bool headers_from_json(const boost::json::object &obj, std::unordered_map<std::string, std::string> &headers)
    {
      if (auto it = obj.if_contains("headers"); it && it->is_object())
      {
        headers = json_object_to_map(it->as_object());
        return true;
      }
      return false;
    }
    
  }
} // namespace agreement::agreement_detail_processing
