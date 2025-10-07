#pragma once
#include <boost/json.hpp>
#include <boost/asio.hpp>
#include <string>
#include <iostream>
#include <string_view>

namespace protocol
{
  class json
  {
  private:
    boost::json::value _value;                 // JSON值
    mutable std::string _cached_string;        // 缓存的JSON字符串
    mutable bool _string_cache_valid = false;  // 字符串缓存是否有效

  private:
    /**
     * @brief 无效化字符串缓存
     */
    void _invalidate_cache() const
    {
      _string_cache_valid = false;
    }
  public:
    json() : _value(boost::json::object{}) {}

    explicit json(const boost::json::value &value) : _value(value) {}

    explicit json(boost::json::value &&value) noexcept : _value(std::move(value)) {}

    explicit json(const std::string &json_str)
    {
      from_string(json_str);
    }

    explicit json(std::string_view json_str)
    {
      from_string(json_str);
    }

    // 拷贝和移动语义
    json(const json &other) : _value(other._value) {}

    json(json &&other) noexcept
    : _value(std::move(other._value)), _cached_string(std::move(other._cached_string)), _string_cache_valid(other._string_cache_valid)
    {
      other._string_cache_valid = false;
    }

    json &operator=(const json &other)
    {
      if (this != &other)
      {
        _value = other._value;
        _invalidate_cache();
      }
      return *this;
    }

    json &operator=(json &&other) noexcept
    {
      if (this != &other)
      {
        _value = std::move(other._value);
        _cached_string = std::move(other._cached_string);
        _string_cache_valid = other._string_cache_valid;
        other._string_cache_valid = false;
      }
      return *this;
    }
    /**
     * @brief 从字符串解析`JSON`
     * @param json_str `JSON`字符串
     * @return 解析是否成功
     */
    bool from_string(std::string_view json_str) noexcept
    {
      try
      {
        _value = boost::json::parse(json_str);
        _invalidate_cache();
        return true;
      }
      catch (...)
      {
        return false;
      }
    }

    /**
     * @brief 转换为字符串
     * @return JSON字符串表示
     */
    const std::string &to_string() const
    {
      if (!_string_cache_valid)
      {
        _cached_string = boost::json::serialize(_value);
        _string_cache_valid = true;
      }
      return _cached_string;
    }
    /**
     * @brief 获取原始boost::json::value引用
     * @return boost::json::value引用
     */
    const boost::json::value &value() const noexcept { return _value; }

    boost::json::value &value() noexcept
    {
      _invalidate_cache();
      return _value;
    }

    /**
     * @brief 类型安全的值获取
     * @tparam current_header_t 目标类型
     * @param key 键名
     * @param default_value 默认值
     * @return 获取的值或默认值
     */
    template <typename current_header_t>
    current_header_t get(const std::string &key, const current_header_t &default_value = current_header_t{}) const noexcept
    {
      try
      {
        if (!_value.is_object())
          return default_value;

        const auto &obj = _value.as_object();
        auto it = obj.find(key);
        if (it == obj.end())
          return default_value;

        if constexpr (std::is_same_v<current_header_t, std::string>)
        {
          if (it->value().is_string())
            return std::string(it->value().as_string());
        }
        else if constexpr (std::is_integral_v<current_header_t>)
        {
          if (it->value().is_int64())
            return static_cast<current_header_t>(it->value().as_int64());
          if (it->value().is_uint64())
            return static_cast<current_header_t>(it->value().as_uint64());
        }
        else if constexpr (std::is_floating_point_v<current_header_t>)
        {
          if (it->value().is_double())
            return static_cast<current_header_t>(it->value().as_double());
        }
        else if constexpr (std::is_same_v<current_header_t, bool>)
        {
          if (it->value().is_bool())
            return it->value().as_bool();
        }
        return default_value;
      }
      catch (...)
      {
        return default_value;
      }
    }

    /**
     * @brief 类型安全的值设置
     * @tparam current_header_t 值类型
     * @param key 键名
     * @param value 值
     */
    template <typename current_header_t>
    void set(const std::string &key, const current_header_t &value)
    {
      if (!_value.is_object())
        _value = boost::json::object{};

      auto &obj = _value.as_object();

      if constexpr (std::is_same_v<current_header_t, std::string>)
      {
        obj[key] = value;
      }
      else if constexpr (std::is_integral_v<current_header_t>)
      {
        if constexpr (std::is_signed_v<current_header_t>)
          obj[key] = static_cast<std::int64_t>(value);
        else
          obj[key] = static_cast<std::uint64_t>(value);
      }
      else if constexpr (std::is_floating_point_v<current_header_t>)
      {
        obj[key] = static_cast<double>(value);
      }
      else if constexpr (std::is_same_v<current_header_t, bool>)
      {
        obj[key] = value;
      }
      else
      {
        // 对于其他类型，尝试转换为字符串
        obj[key] = std::to_string(value);
      }

      _invalidate_cache();
    }

    /**
     * @brief 检查是否包含指定键
     * @param key 键名
     * @return 是否包含该键
     */
    bool contains(const std::string &key) const noexcept
    {
      if (!_value.is_object())
        return false;

      const auto &obj = _value.as_object();
      return obj.find(key) != obj.end();
    }

    /**
     * @brief 移除指定键
     * @param key 键名
     * @return 是否成功移除
     */
    bool remove(const std::string &key) noexcept
    {
      if (!_value.is_object())
        return false;

      auto &obj = _value.as_object();
      auto it = obj.find(key);
      if (it != obj.end())
      {
        obj.erase(it);
        _invalidate_cache();
        return true;
      }
      return false;
    }

    /**
     * @brief 清空所有内容
     */
    void clear() noexcept
    {
      _value = boost::json::object{};
      _invalidate_cache();
    }

    /**
     * @brief 获取对象大小（键值对数量）
     * @return 大小
     */
    std::size_t size() const noexcept
    {
      if (_value.is_object())
        return _value.as_object().size();
      if (_value.is_array())
        return _value.as_array().size();
      return 0;
    }

    /**
     * @brief 检查是否为空
     * @return 是否为空
     */
    bool empty() const noexcept
    {
      return size() == 0;
    }

    bool operator==(const json &other) const noexcept
    {
      return _value == other._value;
    }

    bool operator!=(const json &other) const noexcept
    {
      return !(*this == other);
    }
  };
} // end namespace protocol