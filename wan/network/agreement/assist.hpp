/**
  * @file assist.hpp
  * @brief 协议辅助套件定义
  * @details 提供协议类型、校验类型、协议头等辅助功能
  */
#pragma once
#include <string>
#include <optional>
#include <unordered_map>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <boost/json.hpp>
#include "./json.hpp"
#include "../crypt/encryption.hpp"



namespace protocol
{
    namespace assist {}
} // end namespace protocol

namespace protocol::assist
{
    /**
      * @brief 协议类型枚举
      * @details 定义支持的协议类型，可扩展
      */
    enum class protocol_type : std::uint8_t
    {
        JSON_RPC,      // `JSON-RPC`协议
        WEBSOCKET,     // `WebSocket`协议
        CUSTOM_TCP,    // 自定义`TCP`协议
        BINARY_STREAM, // 二进制流协议
        USER_DEFINED   // 用户自定义协议
    }; // end enum class protocol_type

    /**
      * @brief 数据完整性校验类型
      * @details 支持多种校验算法
      */
    enum class checksum_type : std::uint8_t
    {
        CRC32,
        MD5,
        SHA256,
        CUSTOM
    }; // end enum class checksum_type

    /**
      * @brief 协议头基类
      * @details 提供协议头的基础接口，支持自定义协议类型
      */
    class protocol_header
    {
    protected:
        std::uint32_t version_ = 1;                               // 协议版本
        std::uint32_t checksum_value_ = 0;                        // 校验值
        std::uint64_t content_length_ = 0;                        // 内容长度
        checksum_type checksum_type_ = checksum_type::CRC32;      // 校验类型
        std::unordered_map<std::string, std::string> headers_;    // 头部字段
        protocol_type protocol_type_ = protocol_type::CUSTOM_TCP; // 协议类型

    protected:
        /**
          * @brief 计算数据校验值
          * @param data 待校验数据
          * @return 校验值
          */
        virtual std::uint32_t calculate_check_code(std::string_view data) const
        {
            switch (checksum_type_)
            {
            case checksum_type::CRC32:
                return encryption::CRC32(std::string(data), data.size());
            case checksum_type::MD5:
            {
                auto md5_hex = encryption::umbrage_hash::MD5(std::string(data));
                std::uint32_t result = 0;
                std::from_chars(md5_hex.data(), md5_hex.data() + 8, result, 16);
                return result;
            }
            case checksum_type::SHA256:
            {
                auto sha256_hex = encryption::umbrage_hash::SHA256(std::string(data));
                std::uint32_t result = 0;
                std::from_chars(sha256_hex.data(), sha256_hex.data() + 8, result, 16);
                return result;
            }
            default:
                return 0;
            }
        }

    public:
        virtual ~protocol_header() = default;
        protocol_header() { headers_.reserve(16); } // 预分配16个头部字段

        protocol_type get_protocol_type() const noexcept { return protocol_type_; }
        void set_protocol_type(protocol_type type) noexcept { protocol_type_ = type; }

        checksum_type get_checksum_type() const noexcept { return checksum_type_; }
        void set_checksum_type(checksum_type type) noexcept { checksum_type_ = type; }

        std::uint32_t get_checksum_value() const noexcept { return checksum_value_; }
        void set_checksum_value(std::uint32_t value) noexcept { checksum_value_ = value; }

        std::uint64_t get_content_length() const noexcept { return content_length_; }
        void set_content_length(std::uint64_t length) noexcept { content_length_ = length; }

        std::uint32_t get_version() const noexcept { return version_; }
        void set_version(std::uint32_t version) noexcept { version_ = version; }

        // 清空所有头部字段
        void clear_headers() noexcept { headers_.clear(); }

        // 移除头部字段
        bool remove_header(const std::string &key) { return headers_.erase(key) > 0; }

        // 设置头部字段 `key` `value`
        void set_header(const std::string &key, const std::string &value) { headers_[key] = value; }

        //  获取所有头部字段
        const std::unordered_map<std::string, std::string> &get_headers() const noexcept { return headers_; }
        /**
          * @brief 获取头部字段
          * @param key 键
          * @return 值的可选对象
          */
        std::optional<std::string> get_header(const std::string &key) const
        {
            auto it = headers_.find(key);
            if (it != headers_.end())
                return it->second;
            return std::nullopt;
        }
        /**
          * @brief 序列化为字符串
          * @return 序列化后的字符串
          */
        virtual std::string to_string() const = 0;

        /**
          * @brief 从字符串反序列化
          * @param data 字符串数据
          * @return 是否成功
          */
        virtual bool from_string(std::string_view data) = 0;

        /**
          * @brief 计算并设置校验值
          * @param content 内容数据
          * @return 计算得到的校验值
          */
        virtual std::uint32_t calculate_and_set_checksum(std::string_view content)
        {
            content_length_ = content.size();
            checksum_value_ = calculate_check_code(content);
            return checksum_value_;
        }
        /**
          * @brief 验证数据完整性
          * @param content 内容数据
          * @return 验证是否通过
          */
        virtual bool verify_integrity(std::string_view content) const
        {
            if (content.size() != content_length_)
                return false;

            return calculate_check_code(content) == checksum_value_;
        }

          /**
          * @brief 转换为`JSON`
          * @return `JSON`对象
          */
        virtual protocol::json to_json() const
        {
            protocol::json json_object;
            json_object.set("protocol_type", static_cast<std::uint8_t>(protocol_type_));
            json_object.set("checksum_type", static_cast<std::uint8_t>(checksum_type_));
            json_object.set("checksum_value", checksum_value_);
            json_object.set("content_length", content_length_);
            json_object.set("version", version_);
            for (const auto &[key, value] : headers_)
            {
                json_object.set("header_" + key, value);
            }
            return json_object;
        }
        /**
          * @brief 从`JSON`反序列化
          * @param json_object `JSON`对象
          * @return 是否成功
          */
        virtual bool from_json(const protocol::json &json_object)
        {
            try
            {
                version_ = json_object.get<std::uint32_t>("version", 1);
                content_length_ = json_object.get<std::uint64_t>("content_length", 0);
                checksum_value_ = json_object.get<std::uint32_t>("checksum_value", 0);
                protocol_type_ = static_cast<protocol_type>(json_object.get<std::uint8_t>("protocol_type", 1));
                checksum_type_ = static_cast<checksum_type>(json_object.get<std::uint8_t>("checksum_type", 1));
                headers_.clear();

                const auto &value = json_object.value();
                if (value.is_object())
                {
                    const auto &obj = value.as_object();
                    for (const auto &[key, val] : obj)
                    {
                        std::string_view key_view(key);
                        if (key_view.starts_with("header_") && val.is_string())
                        {
                            // 使用string_view避免不必要的字符串拷贝
                            std::string_view header_key_view = key_view.substr(7); // 移除"header_"前缀
                            headers_.emplace(std::string(header_key_view), std::string(val.as_string()));
                        }
                    }
                }
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    }; // end class protocol_header
} // end namespace aux