/**
  * @file Protocol.hpp
  * @brief 协议定义
  * @details 提供基础协议头、协议体、协议转换等基础组件
  */
#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <concepts> //模板约束
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <format>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <boost/json.hpp>
#include "./Json.hpp"
#include "./Assist.hpp"
#include "../Crypt/Encryption.hpp"

namespace Protocol
{
    /** @brief 单条协议消息允许的最大字节数。 */
    inline constexpr std::size_t MaxProtocolMessageSize = 1024 * 1024;

    /**
     * @brief 检查类型是否满足协议头接口。
     * @tparam HeaderT 待检查的协议头类型。
     */
    template <class HeaderT>
    concept HeaderConstraint = requires(HeaderT Header,std::string_view Data,const Protocol::Json& JsonObject,std::string_view Content)
    {
        { Header.ToString() } -> std::same_as<std::string>;
        { Header.FromString(Data) } -> std::same_as<bool>;
        { Header.ToJson() } -> std::same_as<Protocol::Json>;
        { Header.FromJson(JsonObject) } -> std::same_as<bool>;
        { Header.VerifyIntegrity(Content) } -> std::same_as<bool>;
        { Header.CalculateAndSetChecksum(Content) } -> std::same_as<std::uint32_t>;
    };
    /**
     * @brief TCP 协议请求头。
     * @details 方法、目标和 User-Agent 均会在设置时进行长度与换行符校验。
     */
    class RequestHeader : public Assist::ProtocolHeader
    {
    private:
        /** @brief 请求方法文本。 */
        std::string MemberMethod;
        /** @brief 请求目标路径。 */
        std::string MemberTarget;
        /** @brief 客户端 User-Agent 文本。 */
        std::string MemberUserAgent;
        /** @brief 请求头记录的系统时间。 */
        std::chrono::system_clock::time_point MemberTimestamp;
    private:
        /**
         * @brief 将头字段追加到输出字符串。
         * @param Out 接收序列化内容的字符串。
          */
        void SerializeHeadersToString(std::string &Out) const
        {
            std::vector<std::string> Keys;
            Keys.reserve(MemberHeaders.size());
            for (const auto &[key, value] : MemberHeaders)
                Keys.push_back(key);
            std::sort(Keys.begin(), Keys.end());
            for (const std::string &Key : Keys)
            {
                const std::string &Value = MemberHeaders.at(Key);
                Out.append(Key);
                Out.append(": ");
                Out.append(Value);
                Out.append("\r\n");
            }
        }
    public:
        /** @brief 创建请求头并初始化时间戳。 */
        RequestHeader()
        {
            MemberTimestamp = std::chrono::system_clock::now();
            MemberProtocolType = Assist::ProtocolType::CustomTcp;
        }

        /**
         * @brief 获取请求方法。
         * @return 请求方法的只读引用。
         */
        const std::string &GetMethod() const noexcept { return MemberMethod; }

        /**
         * @brief 设置请求方法。
         * @param Method 请求方法，长度须为 1 到 10 字节且不能含空白符。
         * @throws std::invalid_argument 方法值不符合约束时抛出。
         */
        void SetMethod(const std::string &Method)
        {
            if (Method.empty() || Method.size() > 10
                || Method.find_first_of(" \t\r\n") != std::string::npos)
            {
                throw std::invalid_argument("invalid protocol method");
            }
            MemberMethod = Method;
        }

        /**
         * @brief 获取请求目标。
         * @return 请求目标的只读引用。
         */
        const std::string &GetTarget() const noexcept { return MemberTarget; }

        /**
         * @brief 设置请求目标。
         * @param Target 非空目标，长度不得超过 2048 字节且不能含换行符。
         * @throws std::invalid_argument 目标值不符合约束时抛出。
         */
        void SetTarget(const std::string &Target)
        {
            if (Target.empty() || Target.size() > 2048
                || Target.find_first_of("\r\n") != std::string::npos)
            {
                throw std::invalid_argument("invalid protocol target");
            }
            MemberTarget = Target;
        }

        /**
         * @brief 获取 User-Agent 值。
         * @return User-Agent 的只读引用。
         */
        const std::string &GetUserAgent() const noexcept { return MemberUserAgent; }

        /**
         * @brief 设置 User-Agent 值。
         * @param UserAgent 最多 512 字节且不含换行符的值。
         * @throws std::invalid_argument 值不符合约束时抛出。
         */
        void SetUserAgent(const std::string &UserAgent)
        {
            if (UserAgent.size() > 512
                || UserAgent.find_first_of("\r\n") != std::string::npos)
            {
                throw std::invalid_argument("invalid user agent");
            }
            MemberUserAgent = UserAgent;
        }

        /**
         * @brief 获取请求时间戳。
         * @return 时间戳的只读引用。
         */
        const std::chrono::system_clock::time_point &GetTimestamp() const noexcept { return MemberTimestamp; }

        /**
         * @brief 设置请求时间戳。
         * @param Timestamp 新时间戳。
         */
        void SetTimestamp(const std::chrono::system_clock::time_point &Timestamp) { MemberTimestamp = Timestamp; }

        /**
         * @brief 将请求头序列化为字符串。
         * @return 序列化结果。
         */
        std::string ToString() const override
        {
            std::size_t EstimatedSize = MemberMethod.size() + MemberTarget.size() + 64;
            for (const auto &[key, value] : MemberHeaders)
                EstimatedSize += key.size() + value.size() + 4; // ": " + "\r\n"

            std::string Result;
            Result.reserve(EstimatedSize);

            Result += std::format("{} {} {} {} {} {}\r\n",MemberMethod, MemberTarget, MemberVersion,
            static_cast<std::uint8_t>(MemberChecksumType),MemberChecksumValue, MemberContentLength);

            if (!MemberUserAgent.empty())
                Result += std::format("User-Agent: {}\r\n", MemberUserAgent);
            auto TimestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(MemberTimestamp.time_since_epoch()).count();
            Result += std::format("Timestamp: {}\r\n", TimestampMs);

            SerializeHeadersToString(Result);
            Result += "\r\n";
            return Result;
        }
        /**
         * @brief 从字符串解析请求头。
         * @param Data 待解析的请求头文本。
         * @return 解析成功时为 `true`。
         */
        bool FromString(std::string_view Data) override;

        /**
         * @brief 将请求头转换为 JSON 对象。
         * @return 包含请求头字段的 JSON 对象。
         */
        Protocol::Json ToJson() const override
        {
            Protocol::Json JsonObject = ProtocolHeader::ToJson();
            JsonObject.Set("method", MemberMethod);
            JsonObject.Set("target", MemberTarget);
            JsonObject.Set("user_agent", MemberUserAgent);

            auto TimestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(MemberTimestamp.time_since_epoch()).count();
            JsonObject.Set("timestamp", TimestampMs);

            return JsonObject;
        }
        /**
         * @brief 从 JSON 对象解析请求头。
         * @param JsonObject 输入 JSON 对象。
         * @return 解析成功时为 `true`；失败时对象保持不变。
         */
        bool FromJson(const Protocol::Json &JsonObject) override
        {
            try
            {
                RequestHeader Candidate(*this);
                if (!Candidate.ProtocolHeader::FromJson(JsonObject))
                {
                    return false;
                }
                const auto *Object = JsonObject.Value().if_object();
                if (Object == nullptr)
                {
                    return false;
                }
                auto Method = Object->find("method");
                auto Target = Object->find("target");
                auto UserAgent = Object->find("user_agent");
                auto Timestamp = Object->find("timestamp");
                if (Method == Object->end() || !Method->value().is_string()
                    || Target == Object->end() || !Target->value().is_string()
                    || UserAgent == Object->end() || !UserAgent->value().is_string()
                    || Timestamp == Object->end())
                {
                    return false;
                }

                Candidate.MemberMethod = std::string(Method->value().as_string());
                Candidate.MemberTarget = std::string(Target->value().as_string());
                Candidate.MemberUserAgent = std::string(UserAgent->value().as_string());
                const auto TimestampMs = boost::json::value_to<std::int64_t>(
                    Timestamp->value());
                const auto NowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                if (Candidate.MemberMethod.empty() || Candidate.MemberMethod.size() > 10
                    || Candidate.MemberTarget.empty() || Candidate.MemberTarget.size() > 2048
                    || Candidate.MemberUserAgent.size() > 512
                    || TimestampMs < 0 || TimestampMs > NowMs + 86400000)
                {
                    return false;
                }
                Candidate.MemberTimestamp = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(TimestampMs));
                *this = std::move(Candidate);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    }; // end class RequestHeader

    /** @brief TCP 协议响应头。 */
    class ResponseHeader : public Assist::ProtocolHeader
    {
    private:
        /** @brief 服务端标识文本。 */
        std::string MemberServer;
        /** @brief HTTP 风格响应状态码。 */
        std::uint16_t MemberStatusCode = 200;
        /** @brief 与状态码对应的状态文本。 */
        std::string MemberStatusMessage = "OK";
        /** @brief 响应头记录的系统时间。 */
        std::chrono::system_clock::time_point MemberTimestamp;
    private:
        /**
         * @brief 将头字段追加到输出字符串。
         * @param Out 接收序列化内容的字符串。
          */
        void SerializeHeadersToString(std::string &Out) const
        {
            std::vector<std::string> Keys;
            Keys.reserve(MemberHeaders.size());
            for (const auto &[key, value] : MemberHeaders)
                Keys.push_back(key);
            std::sort(Keys.begin(), Keys.end());
            for (const std::string &Key : Keys)
            {
                const std::string &Value = MemberHeaders.at(Key);
                Out.append(Key);
                Out.append(": ");
                Out.append(Value);
                Out.append("\r\n");
            }
        }
    public:
        /** @brief 创建响应头并初始化时间戳。 */
        ResponseHeader()
        {
            MemberTimestamp = std::chrono::system_clock::now();
            MemberProtocolType = Assist::ProtocolType::CustomTcp;
        }
        /**
         * @brief 获取 HTTP 状态码。
         * @return 当前状态码。
         */
        std::uint16_t GetStatusCode() const noexcept { return MemberStatusCode; }

        /**
         * @brief 设置 HTTP 状态码。
         * @param CodeValue 范围为 100 到 599 的状态码。
         * @throws std::invalid_argument 状态码超出范围时抛出。
         */
        void SetStatusCode(std::uint16_t CodeValue)
        {
            if (CodeValue < 100 || CodeValue > 599)
            {
                throw std::invalid_argument("invalid status code");
            }
            MemberStatusCode = CodeValue;
        }

        /**
         * @brief 获取状态消息。
         * @return 状态消息的只读引用。
         */
        const std::string &GetStatusMessage() const noexcept { return MemberStatusMessage; }

        /**
         * @brief 设置状态消息。
         * @param Message 非空、最多 256 字节且不含换行符的消息。
         * @throws std::invalid_argument 消息不符合约束时抛出。
         */
        void SetStatusMessage(const std::string &Message)
        {
            if (Message.empty() || Message.size() > 256
                || Message.find_first_of("\r\n") != std::string::npos)
            {
                throw std::invalid_argument("invalid status message");
            }
            MemberStatusMessage = Message;
        }

        /**
         * @brief 获取 Server 字段值。
         * @return Server 字段的只读引用。
         */
        const std::string &GetServer() const noexcept { return MemberServer; }

        /**
         * @brief 设置 Server 字段值。
         * @param ServerValue 最多 512 字节且不含换行符的值。
         * @throws std::invalid_argument 值不符合约束时抛出。
         */
        void SetServer(const std::string &ServerValue)
        {
            if (ServerValue.size() > 512
                || ServerValue.find_first_of("\r\n") != std::string::npos)
            {
                throw std::invalid_argument("invalid server value");
            }
            MemberServer = ServerValue;
        }

        /**
         * @brief 获取响应时间戳。
         * @return 时间戳的只读引用。
         */
        const std::chrono::system_clock::time_point &GetTimestamp() const noexcept { return MemberTimestamp; }

        /**
         * @brief 设置响应时间戳。
         * @param Timestamp 新时间戳。
         */
        void SetTimestamp(const std::chrono::system_clock::time_point &Timestamp) { MemberTimestamp = Timestamp; }

        /**
         * @brief 将响应头序列化为字符串。
         * @return 序列化结果。
         */
        std::string ToString() const override
        {
            std::size_t EstimatedSize = MemberStatusMessage.size() + 64;
            for (const auto &[key, value] : MemberHeaders)
                EstimatedSize += key.size() + value.size() + 4;
            std::string Result;
            Result.reserve(EstimatedSize);
            Result += std::format("{} {} {} {} {} {}\r\n",MemberVersion, MemberStatusCode, MemberStatusMessage,
            static_cast<std::uint8_t>(MemberChecksumType),MemberChecksumValue, MemberContentLength);
            if (!MemberServer.empty())
                Result += std::format("Server: {}\r\n", MemberServer);
            auto TimestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(MemberTimestamp.time_since_epoch()).count();
            Result += std::format("Timestamp: {}\r\n", TimestampMs);
            SerializeHeadersToString(Result);
            Result += "\r\n";
            return Result;
        }

        /**
         * @brief 从字符串解析响应头。
         * @param Data 待解析的响应头文本。
         * @return 解析成功时为 `true`。
         */
        bool FromString(std::string_view Data) override;
        /**
         * @brief 将响应头转换为 JSON 对象。
         * @return 包含响应头字段的 JSON 对象。
         */
        Protocol::Json ToJson() const override
        {
            Protocol::Json JsonObject = ProtocolHeader::ToJson();
            JsonObject.Set("status_code", MemberStatusCode);
            JsonObject.Set("status_message", MemberStatusMessage);
            JsonObject.Set("server", MemberServer);
            auto TimestampMs = std::chrono::duration_cast<std::chrono::milliseconds>(MemberTimestamp.time_since_epoch()).count();
            JsonObject.Set("timestamp", TimestampMs);
            return JsonObject;
        }
        /**
         * @brief 从 JSON 对象解析响应头。
         * @param JsonObject 输入 JSON 对象。
         * @return 解析成功时为 `true`；失败时对象保持不变。
         */
        bool FromJson(const Protocol::Json &JsonObject) override
        {
            try
            {
                ResponseHeader Candidate(*this);
                if (!Candidate.ProtocolHeader::FromJson(JsonObject))
                {
                    return false;
                }
                const auto *Object = JsonObject.Value().if_object();
                if (Object == nullptr)
                {
                    return false;
                }
                auto StatusCode = Object->find("status_code");
                auto StatusMessage = Object->find("status_message");
                auto Server = Object->find("server");
                auto Timestamp = Object->find("timestamp");
                if (StatusCode == Object->end() || StatusMessage == Object->end()
                    || Server == Object->end() || Timestamp == Object->end()
                    || !StatusCode->value().is_number()
                    || !StatusMessage->value().is_string()
                    || !Server->value().is_string())
                {
                    return false;
                }

                Candidate.MemberStatusCode = boost::json::value_to<std::uint16_t>(
                    StatusCode->value());
                Candidate.MemberStatusMessage = std::string(
                    StatusMessage->value().as_string());
                Candidate.MemberServer = std::string(Server->value().as_string());
                const auto TimestampMs = boost::json::value_to<std::int64_t>(
                    Timestamp->value());
                const auto NowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                if (Candidate.MemberStatusCode < 100
                    || Candidate.MemberStatusCode > 599
                    || Candidate.MemberStatusMessage.empty()
                    || Candidate.MemberStatusMessage.size() > 256
                    || Candidate.MemberServer.size() > 512
                    || TimestampMs < 0 || TimestampMs > NowMs + 86400000)
                {
                    return false;
                }
                Candidate.MemberTimestamp = std::chrono::system_clock::time_point(
                    std::chrono::milliseconds(TimestampMs));
                *this = std::move(Candidate);
                return true;
            }
            catch (...)
            {
                return false;
            }
        }
    }; // end class ResponseHeader

    /**
     * @brief 带可定制请求头的协议请求。
     * @tparam HeaderT 满足 `HeaderConstraint` 的请求头类型，默认为 `RequestHeader`。
     */
    template <HeaderConstraint HeaderT = RequestHeader>
    class Request
    {
    private:
        /** @brief 请求头；可变以支持缓存校验和更新。 */
        mutable HeaderT MemberHeader;
        /** @brief 请求正文。 */
        std::string MemberMessage;
        /** @brief 序列化后的完整请求缓存。 */
        mutable std::string MemberCachedFull;
        /** @brief 指示完整请求缓存是否有效。 */
        mutable bool MemberFullCacheValid = false;
    private:
        /**
          * @brief 清除缓存
          */
        void InvalidateCache() const noexcept
        {
            MemberFullCacheValid = false;
        }
    public:
        /** @brief 创建默认请求。 */
        Request() = default;

        /**
         * @brief 使用请求头创建请求。
         * @param Header 初始请求头。
         */
        explicit Request(const HeaderT &Header) : MemberHeader(Header) {}

        /**
         * @brief 使用请求头和正文创建请求。
         * @param Header 初始请求头。
         * @param Body 请求正文。
         */
        Request(const HeaderT &Header, const std::string &Body)
        : MemberHeader(Header), MemberMessage(Body) {}

        /**
         * @brief 使用请求头和正文创建请求。
         * @param Header 初始请求头。
         * @param Body 要移动到请求中的正文。
         */
        Request(const HeaderT &Header, std::string &&Body)
        : MemberHeader(Header), MemberMessage(std::move(Body)) {}

        // 拷贝和移动语义
        /**
         * @brief 复制请求。
         * @param Other 源请求。
         */
        Request(const Request &Other)
        : MemberHeader(Other.MemberHeader), MemberMessage(Other.MemberMessage) {}

        /**
         * @brief 移动请求。
         * @param Other 源请求。
         */
        Request(Request &&Other) noexcept
        : MemberHeader(std::move(Other.MemberHeader)), MemberMessage(std::move(Other.MemberMessage)),
        MemberCachedFull(std::move(Other.MemberCachedFull)), MemberFullCacheValid(Other.MemberFullCacheValid)
        {
            Other.MemberFullCacheValid = false;
        }

        /**
         * @brief 复制赋值。
         * @param Other 源请求。
         * @return 当前对象的引用。
         */
        Request &operator=(const Request &Other)
        {
            if (this != &Other)
            {
                MemberHeader = Other.MemberHeader;
                MemberMessage = Other.MemberMessage;
                InvalidateCache();
            }
            return *this;
        }

        /**
         * @brief 移动赋值。
         * @param Other 源请求。
         * @return 当前对象的引用。
         */
        Request &operator=(Request &&Other) noexcept
        {
            if (this != &Other)
            {
                MemberHeader = std::move(Other.MemberHeader);
                MemberMessage = std::move(Other.MemberMessage);
                MemberCachedFull = std::move(Other.MemberCachedFull);
                MemberFullCacheValid = Other.MemberFullCacheValid;
                Other.MemberFullCacheValid = false;
            }
            return *this;
        }
        /**
         * @brief 以只读方式获取请求头。
         * @return 请求头的只读引用。
         */
        const HeaderT &Header() const noexcept { return MemberHeader; }

        /**
         * @brief 获取可修改的请求头。
         * @return 请求头的可写引用。
         * @note 调用时会使完整请求字符串缓存失效。
         */
        HeaderT &Header() noexcept
        {
            InvalidateCache();
            return MemberHeader;
        }

        /**
         * @brief 获取请求正文。
         * @return 正文的只读引用。
         */
        const std::string &Body() const noexcept { return MemberMessage; }

        /**
         * @brief 复制设置请求正文。
         * @param Body 新正文。
         */
        void SetMessage(const std::string &Body)
        {
            MemberMessage = Body;
            InvalidateCache();
        }

        /**
         * @brief 移动设置请求正文。
         * @param Body 要移动的新正文。
         */
        void SetMessage(std::string &&Body)
        {
            MemberMessage = std::move(Body);
            InvalidateCache();
        }
        /**
         * @brief 序列化为完整请求字符串。
         * @return 缓存字符串的只读引用。
         * @note 序列化时会更新请求头中的正文长度和校验值。
         */
        const std::string &ToString() const
        {
            if (!MemberFullCacheValid)
            { // `CalculateAndSetChecksum` 作用： 计算消息体校验码并设置校验码
                MemberHeader.CalculateAndSetChecksum(MemberMessage);

                // 优化字符串拼接性能，预分配内存避免多次重新分配
                const auto& MemberHeaderstr = MemberHeader.ToString();
                MemberCachedFull.clear();
                MemberCachedFull.reserve(MemberHeaderstr.size() + MemberMessage.size());
                MemberCachedFull.append(MemberHeaderstr);
                MemberCachedFull.append(MemberMessage);
                MemberFullCacheValid = true;
            }
            return MemberCachedFull;
        }
        /**
         * @brief 从完整请求字符串解析请求。
         * @param Data 完整的请求文本。
         * @return 解析和完整性验证均成功时为 `true`；失败时对象保持不变。
         */
        bool FromString(std::string_view Data)
        {
            std::size_t MemberHeaderend = Data.find("\r\n\r\n");
            if (MemberHeaderend == std::string_view::npos)
                return false;

            std::string_view MemberHeaderdata = Data.substr(0, MemberHeaderend + 2); // 包含最后的\r\n
            HeaderT CandidateHeader(MemberHeader);
            if (!CandidateHeader.FromString(MemberHeaderdata))
                return false;
            // 提取body
            std::size_t BodyStart = MemberHeaderend + 4; // 跳过 "\r\n\r\n"
            std::string CandidateMessage = BodyStart < Data.size()
                ? std::string(Data.substr(BodyStart))
                : std::string{};
            if (!CandidateHeader.VerifyIntegrity(CandidateMessage))
            {
                return false;
            }
            MemberHeader = std::move(CandidateHeader);
            MemberMessage = std::move(CandidateMessage);
            InvalidateCache();
            return true;
        }
        /**
         * @brief 将请求转换为 JSON 对象。
         * @return 包含 `header` 和 `body` 的 JSON 对象。
         * @note 转换时会更新请求头中的正文长度和校验值。
         */
        Protocol::Json ToJson() const
        {
            Protocol::Json JsonObject;
            MemberHeader.CalculateAndSetChecksum(MemberMessage);
            const auto HeaderJson = MemberHeader.ToJson();
            JsonObject.Set("header", HeaderJson.Value());
            JsonObject.Set("body", MemberMessage);
            return JsonObject;
        }
        /**
         * @brief 从 JSON 对象解析请求。
         * @param JsonObject 输入 JSON 对象。
         * @return 解析和完整性验证均成功时为 `true`；失败时对象保持不变。
         */
        bool FromJson(const Protocol::Json &JsonObject)
        {
            try
            {
                const auto *Object = JsonObject.Value().if_object();
                if (Object == nullptr)
                {
                    return false;
                }
                auto Header = Object->find("header");
                auto Body = Object->find("body");
                if (Header == Object->end() || Body == Object->end()
                    || !Body->value().is_string())
                {
                    return false;
                }
                Protocol::Json HeaderJson(Header->value());
                HeaderT CandidateHeader(MemberHeader);
                if (!CandidateHeader.FromJson(HeaderJson))
                {
                    return false;
                }
                std::string CandidateMessage = std::string(
                    Body->value().as_string());
                if (!CandidateHeader.VerifyIntegrity(CandidateMessage))
                {
                    return false;
                }
                MemberHeader = std::move(CandidateHeader);
                MemberMessage = std::move(CandidateMessage);
                InvalidateCache();
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        /**
         * @brief 验证请求正文长度和校验值。
         * @return 两项检查均通过时为 `true`。
         */
        bool VerifyIntegrity() const
        {
            return MemberHeader.VerifyIntegrity(MemberMessage);
        }
        /**
         * @brief 获取完整请求的序列化字节数。
         * @return 完整请求长度。
         */
        std::size_t Size() const
        {
            return ToString().size();
        }
        /**
         * @brief 检查请求正文是否为空。
         * @return 正文为空时为 `true`。
         */
        bool Empty() const noexcept
        {
            return MemberMessage.empty();
        }

        /**
         * @brief 比较请求头和正文是否相等。
         * @param Other 要比较的请求。
         * @return 两者相等时为 `true`。
         */
        bool operator==(const Request &Other) const
        {
            return MemberHeader.ToString() == Other.MemberHeader.ToString() && MemberMessage == Other.MemberMessage;
        }

        /**
         * @brief 比较请求头或正文是否不等。
         * @param Other 要比较的请求。
         * @return 两者不等时为 `true`。
         */
        bool operator!=(const Request &Other) const
        {
            return !(*this == Other);
        }
    }; // end class Request

    /**
     * @brief 带可定制响应头的协议响应。
     * @tparam HeaderT 满足 `HeaderConstraint` 的响应头类型，默认为 `ResponseHeader`。
     * @note 状态快捷方法仅在 `HeaderT` 为 `ResponseHeader` 时设置状态字段。
     */
    template<HeaderConstraint HeaderT = ResponseHeader>
    class Response
    {
    private:
        /** @brief 响应头；可变以支持缓存校验和更新。 */
        mutable HeaderT MemberHeader;
        /** @brief 响应正文。 */
        std::string MemberMessage;
        /** @brief 序列化后的完整响应缓存。 */
        mutable std::string MemberCachedFull;
        /** @brief 指示完整响应缓存是否有效。 */
        mutable bool MemberFullCacheValid = false;

    private:
        /**
          * @brief 清除缓存
          */
        void InvalidateCache() const noexcept
        {
            MemberFullCacheValid = false;
        }

    public:
        /** @brief 创建默认响应。 */
        Response() = default;

        /**
         * @brief 使用响应头创建响应。
         * @param Header 初始响应头。
         */
        explicit Response(const HeaderT &Header) : MemberHeader(Header) {}

        /**
         * @brief 使用响应头和正文创建响应。
         * @param Header 初始响应头。
         * @param Body 响应正文。
         */
        Response(const HeaderT &Header, const std::string &Body)
        : MemberHeader(Header), MemberMessage(Body) {}

        /**
         * @brief 使用响应头和正文创建响应。
         * @param Header 初始响应头。
         * @param Body 要移动到响应中的正文。
         */
        Response(const HeaderT &Header, std::string &&Body)
        : MemberHeader(Header), MemberMessage(std::move(Body)) {}

        /**
         * @brief 使用状态码、状态消息和正文创建响应。
         * @param StatusCode HTTP 状态码。
         * @param MemberStatusmessage 状态消息。
         * @param Body 响应正文。
         * @note 仅当 `HeaderT` 为 `ResponseHeader` 时会设置状态码和状态消息。
         */
        Response(std::uint16_t StatusCode, const std::string &MemberStatusmessage, const std::string &Body = "")
        : MemberMessage(Body)
        {
            if constexpr (std::is_same_v<HeaderT, ResponseHeader>)
            {
                MemberHeader.SetStatusCode(StatusCode);
                MemberHeader.SetStatusMessage(MemberStatusmessage);
            }
        }
        /**
         * @brief 复制响应。
         * @param Other 源响应。
         */
        Response(const Response &Other)
        : MemberHeader(Other.MemberHeader), MemberMessage(Other.MemberMessage) {}

        /**
         * @brief 移动响应。
         * @param Other 源响应。
         */
        Response(Response &&Other) noexcept
        : MemberHeader(std::move(Other.MemberHeader)), MemberMessage(std::move(Other.MemberMessage)), MemberCachedFull(std::move(Other.MemberCachedFull)), MemberFullCacheValid(Other.MemberFullCacheValid)
        {
            Other.MemberFullCacheValid = false;
        }

        /**
         * @brief 复制赋值响应。
         * @param Other 源响应。
         * @return 当前对象的引用。
         */
        Response &operator=(const Response &Other)
        {
            if (this != &Other)
            {
                MemberHeader = Other.MemberHeader;
                MemberMessage = Other.MemberMessage;
                InvalidateCache();
            }
            return *this;
        }

        /**
         * @brief 移动赋值响应。
         * @param Other 源响应。
         * @return 当前对象的引用。
         */
        Response &operator=(Response &&Other) noexcept
        {
            if (this != &Other)
            {
                MemberHeader = std::move(Other.MemberHeader);
                MemberMessage = std::move(Other.MemberMessage);
                MemberCachedFull = std::move(Other.MemberCachedFull);
                MemberFullCacheValid = Other.MemberFullCacheValid;
                Other.MemberFullCacheValid = false;
            }
            return *this;
        }
        /**
         * @brief 以只读方式获取响应头。
         * @return 响应头的只读引用。
         */
        const HeaderT &Header() const noexcept { return MemberHeader; }

        /**
         * @brief 获取可修改的响应头。
         * @return 响应头的可写引用。
         * @note 调用时会使完整响应字符串缓存失效。
         */
        HeaderT &Header() noexcept
        {
            InvalidateCache();
            return MemberHeader;
        }

        /**
         * @brief 获取响应正文。
         * @return 正文的只读引用。
         */
        const std::string &Body() const noexcept { return MemberMessage; }

        /**
         * @brief 复制设置响应正文。
         * @param Body 新正文。
         */
        void SetMessage(const std::string &Body)
        {
            MemberMessage = Body;
            InvalidateCache();
        }

        /**
         * @brief 移动设置响应正文。
         * @param Body 要移动的新正文。
         */
        void SetMessage(std::string &&Body)
        {
            MemberMessage = std::move(Body);
            InvalidateCache();
        }

        // 便利方法（仅适用于 ResponseHeader ） 如果类型不符合 则忽略函数

        /**
         * @brief 获取状态码。
         * @tparam CurrentHeadert 须为 `ResponseHeader`。
         * @return HTTP 状态码。
         */
        template <typename CurrentHeadert = HeaderT>
        std::enable_if_t<std::is_same_v<CurrentHeadert, ResponseHeader>, std::uint16_t>
        GetStatusCode() const noexcept
        {
            return MemberHeader.GetStatusCode();
        }

        /**
         * @brief 设置状态码。
         * @tparam CurrentHeadert 须为 `ResponseHeader`。
         * @param Code 新状态码。
         * @throws std::invalid_argument 状态码不在 100 到 599 范围内时抛出。
         */
        template <typename CurrentHeadert = HeaderT>
        std::enable_if_t<std::is_same_v<CurrentHeadert, ResponseHeader>, void>
        SetStatusCode(std::uint16_t Code)
        {
            MemberHeader.SetStatusCode(Code);
            InvalidateCache();
        }

        /**
         * @brief 获取状态消息。
         * @tparam CurrentHeadert 须为 `ResponseHeader`。
         * @return 状态消息的只读引用。
         */
        template <typename CurrentHeadert = HeaderT>
        std::enable_if_t<std::is_same_v<CurrentHeadert, ResponseHeader>, const std::string &>
        GetStatusMessage() const noexcept
        {
            return MemberHeader.GetStatusMessage();
        }

        /**
         * @brief 设置状态消息。
         * @tparam CurrentHeadert 须为 `ResponseHeader`。
         * @param Message 新状态消息。
         * @throws std::invalid_argument 消息不符合 `ResponseHeader` 约束时抛出。
         */
        template <typename CurrentHeadert = HeaderT>
        std::enable_if_t<std::is_same_v<CurrentHeadert, ResponseHeader>, void>
        SetStatusMessage(const std::string &Message)
        {
            MemberHeader.SetStatusMessage(Message);
            InvalidateCache();
        }

        /**
         * @brief 序列化为完整响应字符串。
         * @return 缓存字符串的只读引用。
         * @note 序列化时会更新响应头中的正文长度和校验值。
         */
        const std::string &ToString() const
        {
            if (!MemberFullCacheValid)
            {
                // 先计算并设置校验值
                MemberHeader.CalculateAndSetChecksum(MemberMessage);

                // 优化字符串拼接性能，预分配内存避免多次重新分配
                const auto& MemberHeaderstr = MemberHeader.ToString();
                MemberCachedFull.clear();
                MemberCachedFull.reserve(MemberHeaderstr.size() + MemberMessage.size());
                MemberCachedFull.append(MemberHeaderstr);
                MemberCachedFull.append(MemberMessage);
                MemberFullCacheValid = true;
            }
            return MemberCachedFull;
        }

        /**
         * @brief 从完整响应字符串解析响应。
         * @param Data 完整的响应文本。
         * @return 解析和完整性验证均成功时为 `true`；失败时对象保持不变。
         */
        bool FromString(std::string_view Data)
        {
            // 查找头部结束标志
            std::size_t MemberHeaderend = Data.find("\r\n\r\n");
            if (MemberHeaderend == std::string_view::npos)
                return false;

            // 解析头部
            std::string_view MemberHeaderdata = Data.substr(0, MemberHeaderend + 2); // 包含最后的\r\n
            HeaderT CandidateHeader(MemberHeader);
            if (!CandidateHeader.FromString(MemberHeaderdata))
                return false;

            // 提取body
            std::size_t BodyStart = MemberHeaderend + 4; // 跳过 "\r\n\r\n"
            std::string CandidateMessage = BodyStart < Data.size()
                ? std::string(Data.substr(BodyStart))
                : std::string{};
            if (!CandidateHeader.VerifyIntegrity(CandidateMessage))
            {
                return false;
            }
            MemberHeader = std::move(CandidateHeader);
            MemberMessage = std::move(CandidateMessage);
            InvalidateCache();
            return true;
        }

        /**
         * @brief 将响应转换为 JSON 对象。
         * @return 包含 `header` 和 `body` 的 JSON 对象。
         * @note 转换时会更新响应头中的正文长度和校验值。
         */
        Protocol::Json ToJson() const
        {
            Protocol::Json JsonObject;
            MemberHeader.CalculateAndSetChecksum(MemberMessage);
            const auto HeaderJson = MemberHeader.ToJson();
            JsonObject.Set("header", HeaderJson.Value());
            JsonObject.Set("body", MemberMessage);
            return JsonObject;
        }
        /**
         * @brief 从 JSON 对象解析响应。
         * @param JsonObject 输入 JSON 对象。
         * @return 解析和完整性验证均成功时为 `true`；失败时对象保持不变。
         */
        bool FromJson(const Protocol::Json &JsonObject)
        {
            try
            {
                const auto *Object = JsonObject.Value().if_object();
                if (Object == nullptr)
                {
                    return false;
                }
                auto Header = Object->find("header");
                auto Body = Object->find("body");
                if (Header == Object->end() || Body == Object->end()
                    || !Body->value().is_string())
                {
                    return false;
                }
                Protocol::Json HeaderJson(Header->value());
                HeaderT CandidateHeader(MemberHeader);
                if (!CandidateHeader.FromJson(HeaderJson))
                {
                    return false;
                }
                std::string CandidateMessage = std::string(
                    Body->value().as_string());
                if (!CandidateHeader.VerifyIntegrity(CandidateMessage))
                {
                    return false;
                }
                MemberHeader = std::move(CandidateHeader);
                MemberMessage = std::move(CandidateMessage);
                InvalidateCache();
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        /**
         * @brief 验证响应正文长度和校验值。
         * @return 两项检查均通过时为 `true`。
         */
        bool VerifyIntegrity() const
        {
            return MemberHeader.VerifyIntegrity(MemberMessage);
        }
        /**
         * @brief 获取完整响应的序列化字节数。
         * @return 完整响应长度。
         */
        std::uint64_t Size() const
        {
            return ToString().size();
        }
        /**
         * @brief 检查响应正文是否为空。
         * @return 正文为空时为 `true`。
         */
        bool Empty() const noexcept
        {
            return MemberMessage.empty();
        }

        /**
         * @brief 比较响应头和正文是否相等。
         * @param Other 要比较的响应。
         * @return 两者相等时为 `true`。
         */
        bool operator==(const Response &Other) const
        {
            return MemberHeader.ToString() == Other.MemberHeader.ToString() && MemberMessage == Other.MemberMessage;
        }

        /**
         * @brief 比较响应头或正文是否不等。
         * @param Other 要比较的响应。
         * @return 两者不等时为 `true`。
         */
        bool operator!=(const Response &Other) const
        {
            return !(*this == Other);
        }

        /**
         * @brief 创建状态为 200 的成功响应。
         * @param Body 响应正文。
         * @param Server Server 字段值。
         * @return 新响应。
         * @note 仅当 `HeaderT` 为 `ResponseHeader` 时会设置 Server 字段。
         */
        static Response CreateSuccess(const std::string &Body = "", const std::string &Server = "")
        {
            Response Resp(200, "OK", Body);
            if constexpr (std::is_same_v<HeaderT, ResponseHeader>)
            {
                if (!Server.empty())
                    Resp.MemberHeader.SetServer(Server);
            }
            return Resp;
        }
        /**
         * @brief 创建指定状态码的错误响应。
         * @param Code HTTP 状态码。
         * @param Message 状态消息。
         * @param Body 响应正文。
         * @return 新响应。
         */
        static Response CreateError(std::uint16_t Code, const std::string &Message, const std::string &Body = "")
        {
            return Response(Code, Message, Body);
        }

        /**
         * @brief 创建 404 响应。
         * @param Body 响应正文。
         * @return 新响应。
         */
        static Response CreateNotFound(const std::string &Body = "not found")
        {
            return Response(404, "not found", Body);
        }

        /**
         * @brief 创建 500 响应。
         * @param Body 响应正文。
         * @return 新响应。
         */
        static Response CreateInternalError(const std::string &Body = "internal server error")
        {
            return Response(500, "internal server error", Body);
        }
    }; // end class Response
    namespace Conversion
    {

    } // end namespace Conversion
} // end namespace Protocol


bool Protocol::RequestHeader::FromString(std::string_view Data)
{
    // 边界检查：确保数据不为空且有最小长度
    if (Data.empty() || Data.size() < 10
        || Data.size() > MaxProtocolMessageSize)
        // 最小合理长度检查和总消息上限
        return false;

    // 异常安全：使用RAII清理，只在成功时提交更改
    std::unordered_map<std::string, std::string> TempHeaders;
    std::string TempMethod, TempTarget, TempUserAgent;
    std::uint32_t TempVersion = 0, TempMemberChecksumValue = 0;
    std::uint64_t TempMemberContentLength = 0;
    Assist::ChecksumType TempMemberChecksumType = Assist::ChecksumType::Crc32;
    std::chrono::system_clock::time_point TempTimestamp;

    std::size_t Pos = 0;

    // 安全的解析函数，添加边界检查
    auto SafeParse = [](std::string_view Sv, auto &Out) noexcept -> bool
    {
        if (Sv.empty() || Sv.size() > 20) // 防止过长的数字字符串
            return false;
        auto Result = std::from_chars(Sv.data(), Sv.data() + Sv.size(), Out);
        return Result.ec == std::errc{} && Result.ptr == Sv.data() + Sv.size();
    };

    // 查找第一行结束位置，添加边界检查
    const auto Le = Data.find("\r\n", Pos);
    if (Le == std::string_view::npos || Le >= Data.size() - 1)
        return false;

    // 解析第一行（请求行）
    std::vector<std::string_view> Parts;
    Parts.reserve(8); // 预分配空间避免重复分配

    for (std::size_t I = Pos, S = Pos; I <= Le; ++I)
    {
        if (I == Le || Data[I] == ' ')
        {
            if (I > S && Parts.size() < 10) // 限制parts数量防止内存攻击
                Parts.push_back(Data.substr(S, I - S));
            S = I + 1;
        }
    }

    // 严格检查parts数量
    if (Parts.size() != 6)
        return false;

    // 验证方法和目标不为空且长度合理
    if (Parts[0].empty() || Parts[0].size() > 10 ||
            Parts[1].empty() || Parts[1].size() > 2048)
        return false;

    TempMethod = std::string(Parts[0]);
    TempTarget = std::string(Parts[1]);

    std::uint8_t CtypeVal;
    if (!SafeParse(Parts[2], TempVersion) || !SafeParse(Parts[3], CtypeVal) ||
            !SafeParse(Parts[4], TempMemberChecksumValue) || !SafeParse(Parts[5], TempMemberContentLength))
        return false;

    // 验证枚举值的有效性（检查上下界）
    if (CtypeVal > static_cast<std::uint8_t>(Assist::ChecksumType::Custom))
        return false;

    TempMemberChecksumType = static_cast<Assist::ChecksumType>(CtypeVal);
    Pos = Le + 2;

    // 安全的trim函数，添加边界检查
    auto SafeTrim = [](std::string_view &Sv) -> bool
    {
        if (Sv.size() > 8192) // 防止过长的字符串
            return false;

        const auto Start = Sv.find_first_not_of(" \t");
        if (Start == std::string_view::npos)
        {
            Sv = "";
            return true;
        }
        const auto End = Sv.find_last_not_of(" \t");
        Sv = Sv.substr(Start, End - Start + 1);
        return true;
    };

    // 解析头部字段，添加循环计数器防止无限循环
    std::size_t MemberHeadercount = 0;
    const std::size_t MaxHeaders = 100; // 限制头部数量
    bool SawTerminator = false;
    bool SawTimestamp = false;
    bool SawUserAgent = false;

    while (Pos < Data.size() && MemberHeadercount < MaxHeaders)
    {
        const auto NextLe = Data.find("\r\n", Pos);
        if (NextLe == std::string_view::npos)
            return false;

        // 防止行过长
        if (NextLe - Pos > 8192)
            return false;

        std::string_view Line = Data.substr(Pos, NextLe - Pos);
        Pos = NextLe + 2;

        if (Line.empty())
        {
            SawTerminator = true;
            break;
        }

        const auto Colon = Line.find(':');
        if (Colon == std::string_view::npos || Colon == 0
            || Colon >= Line.size() - 1)
        {
            return false;
        }
        if (Colon != std::string_view::npos && Colon > 0 && Colon < Line.size() - 1)
        {
            std::string_view K = Line.substr(0, Colon);
            std::string_view V = Line.substr(Colon + 1);

            if (!SafeTrim(K) || !SafeTrim(V))
                return false;

            // 验证键值对的有效性
            if (K.empty() || K.size() > 256 || V.size() > 8192
                || K.find_first_of("\r\n:") != std::string_view::npos
                || V.find_first_of("\r\n") != std::string_view::npos)
                return false;

            std::string Key(K), Val(V);

            if (Key == "User-Agent")
            {
                if (SawUserAgent)
                    return false;
                if (Val.size() > 512) // 限制User-Agent长度
                    return false;
                TempUserAgent = Val;
                SawUserAgent = true;
            }
            else if (Key == "Timestamp")
            {
                if (SawTimestamp)
                    return false;
                std::int64_t Ts;
                if (!SafeParse(V, Ts))
                {
                    return false;
                }
                {
                    // 验证时间戳的合理性（不能是负数或过大）
                    if (Ts < 0 || Ts > std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count() + 86400000) // 不能超过当前时间+1天
                        return false;
                    TempTimestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(Ts));
                }
                SawTimestamp = true;
            }
            else
            {
                if (TempHeaders.size() >= MaxHeaders - 10) // 为特殊头部预留空间
                    return false;
                TempHeaders[Key] = Val;
            }
        }
        ++MemberHeadercount;
    }

    if (!SawTerminator || !SawTimestamp || Pos != Data.size())
        return false;

    // 所有解析成功，提交更改（异常安全）
    MemberHeaders = std::move(TempHeaders);
    MemberMethod = std::move(TempMethod);
    MemberTarget = std::move(TempTarget);
    MemberUserAgent = std::move(TempUserAgent);
    MemberVersion = TempVersion;
    MemberChecksumValue = TempMemberChecksumValue;
    MemberContentLength = TempMemberContentLength;
    MemberChecksumType = TempMemberChecksumType;
    MemberTimestamp = TempTimestamp;

    return true;
}

bool Protocol::ResponseHeader::FromString(std::string_view Data)
{
    // 边界检查：确保数据不为空且有最小长度
    if (Data.empty() || Data.size() < 10
        || Data.size() > MaxProtocolMessageSize)
        return false;

    // 异常安全：使用RAII清理，只在成功时提交更改
    std::unordered_map<std::string, std::string> TempHeaders;
    std::string TempStatusMessage, TempServer;
    std::uint32_t TempVersion = 0, TempMemberChecksumValue = 0;
    std::uint64_t TempMemberContentLength = 0;
    std::uint16_t TempStatusCode = 0;
    Assist::ChecksumType TempMemberChecksumType = Assist::ChecksumType::Crc32;
    std::chrono::system_clock::time_point TempTimestamp;

    std::size_t Pos = 0;

    // 安全的解析函数，添加边界检查
    auto SafeParse = [](std::string_view Sv, auto &Out) noexcept -> bool
    {
        if (Sv.empty() || Sv.size() > 20)
            return false;
        auto Result = std::from_chars(Sv.data(), Sv.data() + Sv.size(), Out);
        return Result.ec == std::errc{} && Result.ptr == Sv.data() + Sv.size();
    };

    // 查找第一行结束位置，添加边界检查
    const auto Le = Data.find("\r\n", Pos);
    if (Le == std::string_view::npos || Le >= Data.size() - 1)
        return false;

    // 解析状态行
    std::vector<std::string_view> Parts;
    Parts.reserve(8);

    for (std::size_t I = Pos, S = Pos; I <= Le; ++I)
    {
        if (I == Le || Data[I] == ' ')
        {
            if (I > S && Parts.size() < 10)
                Parts.push_back(Data.substr(S, I - S));
            S = I + 1;
        }
    }

    // 严格检查parts数量
    if (Parts.size() != 6)
        return false;

    if (!SafeParse(Parts[0], TempVersion) || !SafeParse(Parts[1], TempStatusCode))
        return false;

    // 验证状态码的合理性
    if (TempStatusCode < 100 || TempStatusCode > 599)
        return false;

    // 验证状态消息长度
    if (Parts[2].empty() || Parts[2].size() > 256)
        return false;

    TempStatusMessage = std::string(Parts[2]);

    std::uint8_t CtypeVal;
    if (!SafeParse(Parts[3], CtypeVal) || !SafeParse(Parts[4], TempMemberChecksumValue) ||
            !SafeParse(Parts[5], TempMemberContentLength))
        return false;

    // 验证枚举值的有效性（检查上下界）
    if (CtypeVal > static_cast<std::uint8_t>(Assist::ChecksumType::Custom))
        return false;

    TempMemberChecksumType = static_cast<Assist::ChecksumType>(CtypeVal);
    Pos = Le + 2;

    // 安全的trim函数，添加边界检查
    auto SafeTrim = [](std::string_view &Sv) -> bool
    {
        if (Sv.size() > 8192)
            return false;

        const auto Start = Sv.find_first_not_of(" \t");
        if (Start == std::string_view::npos)
        {
            Sv = "";
            return true;
        }
        const auto End = Sv.find_last_not_of(" \t");
        Sv = Sv.substr(Start, End - Start + 1);
        return true;
    };

    // 解析头部字段，添加循环计数器防止无限循环
    std::size_t MemberHeadercount = 0;
    const std::size_t MaxHeaders = 100;
    bool SawTerminator = false;
    bool SawTimestamp = false;
    bool SawServer = false;

    while (Pos < Data.size() && MemberHeadercount < MaxHeaders)
    {
        const auto NextLe = Data.find("\r\n", Pos);
        if (NextLe == std::string_view::npos)
            return false;

        // 防止行过长
        if (NextLe - Pos > 8192)
            return false;

        std::string_view Line = Data.substr(Pos, NextLe - Pos);
        Pos = NextLe + 2;

        if (Line.empty())
        {
            SawTerminator = true;
            break;
        }

        const auto Colon = Line.find(':');
        if (Colon == std::string_view::npos || Colon == 0
            || Colon >= Line.size() - 1)
        {
            return false;
        }
        if (Colon != std::string_view::npos && Colon > 0 && Colon < Line.size() - 1)
        {
            std::string_view K = Line.substr(0, Colon);
            std::string_view V = Line.substr(Colon + 1);

            if (!SafeTrim(K) || !SafeTrim(V))
                return false;

            // 验证键值对的有效性
            if (K.empty() || K.size() > 256 || V.size() > 8192
                || K.find_first_of("\r\n:") != std::string_view::npos
                || V.find_first_of("\r\n") != std::string_view::npos)
                return false;

            std::string Key(K), Val(V);

            if (Key == "Server")
            {
                if (SawServer)
                    return false;
                if (Val.size() > 512)
                    return false;
                TempServer = Val;
                SawServer = true;
            }
            else if (Key == "Timestamp")
            {
                if (SawTimestamp)
                    return false;
                std::int64_t Ts;
                if (!SafeParse(V, Ts))
                {
                    return false;
                }
                {
                    // 验证时间戳的合理性
                    if (Ts < 0 || Ts > std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count() + 86400000)
                        return false;
                    TempTimestamp = std::chrono::system_clock::time_point(std::chrono::milliseconds(Ts));
                }
                SawTimestamp = true;
            }
            else
            {
                if (TempHeaders.size() >= MaxHeaders - 10)
                    return false;
                TempHeaders[Key] = Val;
            }
        }
        ++MemberHeadercount;
    }

    if (!SawTerminator || !SawTimestamp || Pos != Data.size())
        return false;

    // 所有解析成功，提交更改（异常安全）
    MemberHeaders = std::move(TempHeaders);
    MemberStatusMessage = std::move(TempStatusMessage);
    MemberServer = std::move(TempServer);
    MemberVersion = TempVersion;
    MemberStatusCode = TempStatusCode;
    MemberChecksumValue = TempMemberChecksumValue;
    MemberContentLength = TempMemberContentLength;
    MemberChecksumType = TempMemberChecksumType;
    MemberTimestamp = TempTimestamp;

    return true;
}
