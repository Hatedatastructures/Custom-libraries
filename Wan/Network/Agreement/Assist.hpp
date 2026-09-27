/**
  * @file Assist.hpp
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
#include <type_traits>
#include <stdexcept>
#include <boost/json.hpp>
#include "./Json.hpp"
#include "../Crypt/Encryption.hpp"



namespace Protocol
{
    namespace Assist {}
} // end namespace Protocol

namespace Protocol::Assist
{
    /** @brief 协议头记录的协议类别。 */
    enum class ProtocolType : std::uint8_t
    {
        JsonRpc, ///< JSON-RPC 协议。
        WebSocket, ///< WebSocket 协议。
        CustomTcp, ///< 自定义 TCP 协议。
        BinaryStream, ///< 二进制流协议。
        UserDefined ///< 用户自定义协议。
    }; // end enum class ProtocolType

    /** @brief 协议头使用的数据校验算法。 */
    enum class ChecksumType : std::uint8_t
    {
        Crc32, ///< CRC-32 校验。
        Md5, ///< MD5 摘要。
        Sha256, ///< SHA-256 摘要。
        Custom ///< 自定义校验类型。
    }; // end enum class ChecksumType

    /**
      * @brief 协议头基类
      * @details 提供协议头的基础接口，支持自定义协议类型
      */
    class ProtocolHeader
    {
    protected:
        /** @brief 协议版本号，默认为 1。 */
        std::uint32_t MemberVersion = 1;
        /** @brief 当前正文的校验值。 */
        std::uint32_t MemberChecksumValue = 0;
        /** @brief 当前正文的字节数。 */
        std::uint64_t MemberContentLength = 0;
        /** @brief 当前正文使用的校验算法。 */
        ChecksumType MemberChecksumType = ChecksumType::Crc32;
        /** @brief 协议头字段。 */
        std::unordered_map<std::string, std::string> MemberHeaders;
        /** @brief 当前消息的协议类别。 */
        ProtocolType MemberProtocolType = ProtocolType::CustomTcp;

    protected:
        /**
         * @brief 计算数据校验值。
         * @param Data 待校验数据。
         * @return 校验值。
         */
        virtual std::uint32_t CalculateCheckCode(std::string_view Data) const
        {
            switch (MemberChecksumType)
            {
            case ChecksumType::Crc32:
                return Encryption::Crc32(std::string(Data), Data.size());
            case ChecksumType::Md5:
            {
                auto Md5Hex = Encryption::UmbrageHash::Md5(std::string(Data));
                std::uint32_t Result = 0;
                std::from_chars(Md5Hex.data(), Md5Hex.data() + 8, Result, 16);
                return Result;
            }
            case ChecksumType::Sha256:
            {
                auto Sha256Hex = Encryption::UmbrageHash::Sha256(std::string(Data));
                std::uint32_t Result = 0;
                std::from_chars(Sha256Hex.data(), Sha256Hex.data() + 8, Result, 16);
                return Result;
            }
            default:
                return 0;
            }
        }

    public:
        /** @brief 销毁协议头对象。 */
        virtual ~ProtocolHeader() = default;

        /** @brief 创建默认协议头。 */
        ProtocolHeader() { MemberHeaders.reserve(16); } // 预分配16个头部字段

        /**
         * @brief 获取协议类型。
         * @return 当前协议类型。
         */
        ProtocolType GetProtocolType() const noexcept { return MemberProtocolType; }
        /**
         * @brief 设置协议类型。
         * @param TypeValue 新协议类型。
         */
        void SetProtocolType(ProtocolType TypeValue) noexcept { MemberProtocolType = TypeValue; }

        /**
         * @brief 获取校验算法类型。
         * @return 当前校验算法类型。
         */
        ChecksumType GetChecksumType() const noexcept { return MemberChecksumType; }
        /**
         * @brief 设置校验算法类型。
         * @param TypeValue 校验算法类型。
         * @throws std::invalid_argument 值超出已定义枚举范围时抛出。
         */
        void SetChecksumType(ChecksumType TypeValue)
        {
            if (static_cast<std::uint8_t>(TypeValue)
                > static_cast<std::uint8_t>(ChecksumType::Custom))
            {
                throw std::invalid_argument("invalid checksum type");
            }
            MemberChecksumType = TypeValue;
        }

        /**
         * @brief 获取当前校验值。
         * @return 当前校验值。
         */
        std::uint32_t GetChecksumValue() const noexcept { return MemberChecksumValue; }
        /**
         * @brief 设置校验值。
         * @param Value 新校验值。
         */
        void SetChecksumValue(std::uint32_t Value) noexcept { MemberChecksumValue = Value; }

        /**
         * @brief 获取正文长度。
         * @return 当前正文长度（字节）。
         */
        std::uint64_t GetContentLength() const noexcept { return MemberContentLength; }
        /**
         * @brief 设置正文长度。
         * @param Length 正文长度（字节）。
         */
        void SetContentLength(std::uint64_t Length) noexcept { MemberContentLength = Length; }

        /**
         * @brief 获取协议版本。
         * @return 当前协议版本。
         */
        std::uint32_t GetVersion() const noexcept { return MemberVersion; }
        /**
         * @brief 设置协议版本。
         * @param VersionValue 新协议版本。
         */
        void SetVersion(std::uint32_t VersionValue) noexcept { MemberVersion = VersionValue; }

        /** @brief 清空所有头字段，不修改协议元数据。 */
        void ClearHeaders() noexcept { MemberHeaders.clear(); }

        /**
         * @brief 移除指定头字段。
         * @param Key 要移除的字段名。
         * @return 找到并移除字段时为 `true`。
         */
        bool RemoveHeader(const std::string &Key) { return MemberHeaders.erase(Key) > 0; }

        /**
         * @brief 设置头字段。
         * @param Key 字段名。
         * @param Value 字段值。
         * @throws std::invalid_argument 字段名或字段值违反协议约束时抛出。
         */
        void SetHeader(const std::string &Key, const std::string &Value)
        {
            if (Key.empty() || Key == "User-Agent" || Key == "Timestamp"
                || Key == "Server"
                || Key.find_first_of(":\r\n") != std::string::npos
                || Value.find_first_of("\r\n") != std::string::npos)
            {
                throw std::invalid_argument("invalid protocol header");
            }
            MemberHeaders[Key] = Value;
        }

        /**
         * @brief 获取全部头字段。
         * @return 内部头字段映射的只读引用。
         */
        const std::unordered_map<std::string, std::string> &GetHeaders() const noexcept { return MemberHeaders; }
        /**
         * @brief 按名称查找头字段。
         * @param Key 字段名。
         * @return 找到时返回字段值，否则返回空值。
         */
        std::optional<std::string> GetHeader(const std::string &Key) const
        {
            auto It = MemberHeaders.find(Key);
            if (It != MemberHeaders.end())
                return It->second;
            return std::nullopt;
        }
        /**
         * @brief 将协议头序列化为字符串。
         * @return 序列化结果。
         */
        virtual std::string ToString() const = 0;

        /**
         * @brief 从字符串解析协议头。
         * @param Data 待解析字符串。
         * @return 解析成功时为 `true`。
         */
        virtual bool FromString(std::string_view Data) = 0;

        /**
         * @brief 计算校验值并更新正文长度和校验值字段。
         * @param Content 正文数据。
         * @return 计算得到的校验值。
         */
        virtual std::uint32_t CalculateAndSetChecksum(std::string_view Content)
        {
            MemberContentLength = Content.size();
            MemberChecksumValue = CalculateCheckCode(Content);
            return MemberChecksumValue;
        }
        /**
         * @brief 验证正文长度和校验值。
         * @param Content 待验证正文。
         * @return 长度和校验值均匹配时为 `true`。
         */
        virtual bool VerifyIntegrity(std::string_view Content) const
        {
            if (Content.size() != MemberContentLength)
                return false;

            return CalculateCheckCode(Content) == MemberChecksumValue;
        }

          /**
         * @brief 将协议头转换为 JSON 对象。
         * @return 包含协议元数据和头字段的 JSON 对象。
         */
        virtual Protocol::Json ToJson() const
        {
            Protocol::Json JsonObject;
            JsonObject.Set("protocol_type", static_cast<std::uint8_t>(MemberProtocolType));
            JsonObject.Set("checksum_type", static_cast<std::uint8_t>(MemberChecksumType));
            JsonObject.Set("checksum_value", MemberChecksumValue);
            JsonObject.Set("content_length", MemberContentLength);
            JsonObject.Set("version", MemberVersion);
            for (const auto &[key, value] : MemberHeaders)
            {
                JsonObject.Set("header_" + key, value);
            }
            return JsonObject;
        }
        /**
         * @brief 从 JSON 对象解析协议头。
         * @param JsonObject 输入 JSON 对象。
         * @return 解析成功时为 `true`。
         */
        virtual bool FromJson(const Protocol::Json &JsonObject)
        {
            try
            {
                const auto *Object = JsonObject.Value().if_object();
                if (Object == nullptr)
                {
                    return false;
                }

                auto Read = [&](std::string_view Key, auto &Output) -> bool
                {
                    auto It = Object->find(Key);
                    if (It == Object->end())
                    {
                        return false;
                    }

                    try
                    {
                        using OutputType = std::decay_t<decltype(Output)>;
                        Output = boost::json::value_to<OutputType>(It->value());
                        return true;
                    }
                    catch (...)
                    {
                        return false;
                    }
                };

                std::uint32_t VersionValue = 0;
                std::uint64_t ContentLengthValue = 0;
                std::uint32_t ChecksumValue = 0;
                std::uint8_t ProtocolTypeValue = 0;
                std::uint8_t ChecksumTypeValue = 0;
                if (!Read("version", VersionValue)
                    || !Read("content_length", ContentLengthValue)
                    || !Read("checksum_value", ChecksumValue)
                    || !Read("protocol_type", ProtocolTypeValue)
                    || !Read("checksum_type", ChecksumTypeValue))
                {
                    return false;
                }

                if (ProtocolTypeValue
                        > static_cast<std::uint8_t>(ProtocolType::UserDefined)
                    || ChecksumTypeValue
                        > static_cast<std::uint8_t>(ChecksumType::Custom))
                {
                    return false;
                }

                std::unordered_map<std::string, std::string> Headers;
                Headers.reserve(MemberHeaders.size());
                for (const auto &[Key, Value] : *Object)
                {
                    const std::string_view KeyView(Key);
                    if (!KeyView.starts_with("header_"))
                    {
                        continue;
                    }
                    if (!Value.is_string() || KeyView.size() == 7)
                    {
                        return false;
                    }
                    const auto HeaderKey = KeyView.substr(7);
                    const auto HeaderValue = std::string_view(Value.as_string());
                    if (HeaderKey == "User-Agent" || HeaderKey == "Timestamp"
                        || HeaderKey == "Server"
                        || HeaderKey.find_first_of(":\r\n")
                            != std::string_view::npos
                        || HeaderValue.find_first_of("\r\n")
                            != std::string_view::npos)
                    {
                        return false;
                    }
                    Headers.emplace(std::string(HeaderKey),
                        std::string(HeaderValue));
                }

                MemberVersion = VersionValue;
                MemberContentLength = ContentLengthValue;
                MemberChecksumValue = ChecksumValue;
                MemberProtocolType = static_cast<ProtocolType>(ProtocolTypeValue);
                MemberChecksumType = static_cast<ChecksumType>(ChecksumTypeValue);
                MemberHeaders = std::move(Headers);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    }; // end class ProtocolHeader
} // end namespace aux
