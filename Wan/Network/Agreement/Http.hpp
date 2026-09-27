/**
 * @file Http.hpp
 * @brief HTTP 协议定义
 * @details 提供 HTTP 请求、响应的封装和转换功能
 */
#pragma once
#include <boost/beast/http.hpp>
#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <string>
#include <iostream>
#include <concepts>
#include <sstream>

namespace Protocol
{
    /** @brief 基于 Boost.Beast 的 HTTP 请求与响应封装。 */
    namespace Http
    {
        /** @brief Boost.Beast HTTP 头字段标识类型。 */
        using Field = boost::beast::http::field;

        // 按照 boost.beast 限制：body 必须满足 IsBody，且可实例化为 Request/Response 的正文
        /**
         * @brief 检查正文类型是否可用于 HTTP 请求和响应。
         * @tparam UnderlyingStructure 待检查的正文类型。
         */
        template <class UnderlyingStructure>
        concept BodyStructureConstraint = boost::beast::http::is_body<UnderlyingStructure>::value && requires {
            typename boost::beast::http::request<UnderlyingStructure>;
            typename boost::beast::http::response<UnderlyingStructure>;
        };
        /**
         * @brief 封装 Boost.Beast HTTP 请求。
         * @tparam MessageBody 正文类型，须满足 Boost.Beast 的 HTTP Body 要求。
         * @tparam FieldsContainer 头部字段容器类型，默认为 `boost::beast::http::fields`。
         */
        template <BodyStructureConstraint MessageBody = boost::beast::http::string_body, class FieldsContainer = boost::beast::http::fields>
        class Request
        {
        private:
            /** @brief 被此类包装的 Boost.Beast 请求对象。 */
            boost::beast::http::request<MessageBody, FieldsContainer> MemberReq;

        public:
            /** @brief 请求正文类型。 */
            using MessageBodyType = MessageBody;
            /** @brief 头部字段容器类型。 */
            using FieldsContainerType = FieldsContainer;
            /** @brief 封装的 Boost.Beast 请求类型。 */
            using UnderlyingContainer = boost::beast::http::request<MessageBody, FieldsContainer>;

            /** @brief 创建默认请求。 */
            Request() = default;

            /**
             * @brief 创建指定方法、版本和目标的请求。
             * @param MethodValue HTTP 方法。
             * @param VersionValue HTTP 版本号。
             * @param TargetValue 请求目标。
             */
            Request(boost::beast::http::verb MethodValue,
                unsigned VersionValue, boost::beast::string_view TargetValue)
                : MemberReq{MethodValue, VersionValue, TargetValue} {}

            /**
             * @brief 获取底层请求。
             * @return 底层请求的可写引用。
             */
            UnderlyingContainer &Base() { return MemberReq; }
            /**
             * @brief 以只读方式获取底层请求。
             * @return 底层请求的只读引用。
             */
            UnderlyingContainer const &Base() const { return MemberReq; }

            /**
             * @brief 获取 HTTP 请求方法。
             * @return 当前 HTTP 方法。
             */
            boost::beast::http::verb Method() const { return MemberReq.method(); }
            /**
             * @brief 设置 HTTP 请求方法。
             * @param MemberMethodValue 要设置的方法。
             */
            void Method(boost::beast::http::verb MemberMethodValue) { MemberReq.method(MemberMethodValue); }
            /**
             * @brief 获取 HTTP 方法的字符串表示。
             * @return 指向当前方法文本的非拥有视图。
             */
            boost::beast::string_view MethodString() { return MemberReq.method_string(); }
            /**
             * @brief 以只读方式获取 HTTP 方法的字符串表示。
             * @return 指向当前方法文本的只读视图。
             */
            boost::beast::string_view MethodString() const { return MemberReq.method_string(); }
            /**
             * @brief 获取 HTTP 请求目标。
             * @return 指向当前目标文本的非拥有视图。
             */
            boost::beast::string_view Target() { return MemberReq.target(); }
            /**
             * @brief 设置 HTTP 请求目标。
             * @param MemberTargetValue 要设置的目标。
             */
            void Target(boost::beast::string_view MemberTargetValue) { MemberReq.target(MemberTargetValue); }
            /**
             * @brief 以只读方式获取 HTTP 请求目标。
             * @return 指向当前目标文本的只读视图。
             */
            boost::beast::string_view Target() const { return MemberReq.target(); }
            /**
             * @brief 获取 HTTP 请求版本。
             * @return 当前版本号。
             */
            unsigned Version() const { return MemberReq.version(); }
            /**
             * @brief 设置 HTTP 请求版本。
             * @param MemberVersionValue 要设置的版本号。
             */
            void Version(unsigned MemberVersionValue) { MemberReq.version(MemberVersionValue); }

            /**
             * @brief 获取请求的 Keep-Alive 状态。
             * @return 启用持久连接时为 `true`。
             */
            bool KeepAlive() const { return MemberReq.keep_alive(); }
            /**
             * @brief 设置请求的 Keep-Alive 状态。
             * @param KeepAliveValue 是否启用持久连接。
             */
            void KeepAlive(bool KeepAliveValue) { MemberReq.keep_alive(KeepAliveValue); }

            /**
             * @brief 设置 HTTP 请求头字段。
             * @param Key 头字段标识。
             * @param Value 字段值。
             */
            void Set(boost::beast::http::field Key, std::string Value) { MemberReq.set(Key, Value); }
            /**
             * @brief 获取 HTTP 请求头字段值。
             * @param Key 要查询的头字段标识。
             * @return 字段值。
             */
            std::string At(boost::beast::http::field Key) const { return MemberReq.at(Key); }
            /**
             * @brief 移除 HTTP 请求头字段。
             * @param Key 要移除的头字段标识。
             */
            void Erase(boost::beast::http::field Key) { MemberReq.erase(Key); }

            /**
             * @brief 获取可修改的 HTTP 请求正文。
             * @return 请求正文的可写引用。
             */
            typename MessageBody::value_type &Body() { return MemberReq.body(); }
            /**
             * @brief 以只读方式获取 HTTP 请求正文。
             * @return 请求正文的只读引用。
             */
            typename MessageBody::value_type const &Body() const { return MemberReq.body(); }
            /**
             * @brief 获取请求正文长度上限。
             * @return 正文最大字节数。
             */
            std::uint64_t BodyLimit() const { return MemberReq.body_limit(); }
            /**
             * @brief 设置请求正文长度上限。
             * @param Length 正文最大字节数。
             */
            void BodyLimit(std::uint64_t Length) { MemberReq.body_limit(Length); }
            /**
             * @brief 准备 HTTP 请求正文并更新 `Content-Length` 头字段。
             */
            void PreparePayload() { MemberReq.prepare_payload(); }
            /**
             * @brief 检查请求是否包含 `Content-Length` 头字段。
             * @return 存在该字段时为 `true`。
             */
            bool HasContentLength() const { return MemberReq.has_content_length(); }
            /**
             * @brief 获取 `Content-Length` 表示的正文长度。
             * @return 正文长度（字节）。
             */
            std::uint64_t ContentLength() const { return MemberReq.content_length(); }

            /**
             * @brief 将请求序列化为文本。
             * @return HTTP 请求文本。
             */
            std::string ToString() const
            {
                std::ostringstream Output;
                Output << MemberReq;
                return Output.str();
            }

            /**
             * @brief 从完整 HTTP 请求文本解析请求。
             * @param Data 待解析文本。
             * @return 输入恰好包含一个完整请求且解析成功时为 `true`。
             */
            bool FromString(std::string_view Data)
            {
                boost::beast::error_code Ec;
                boost::beast::http::request_parser<MessageBody> Parser;
                Parser.eager(true);
                Parser.header_limit(64 * 1024);
                Parser.body_limit(64 * 1024 * 1024);
                const auto Consumed = Parser.put(
                    boost::asio::buffer(Data.data(), Data.size()), Ec);
                if (Ec || !Parser.is_done() || Consumed != Data.size())
                    return false;
                MemberReq = Parser.get();
                return true;
            }
        }; // end class Request
        /**
         * @brief 封装 Boost.Beast HTTP 响应。
         * @tparam MessageBody 正文类型，须满足 Boost.Beast 的 HTTP Body 要求。
         * @tparam FieldsContainer 头部字段容器类型，默认为 `boost::beast::http::fields`。
         */
        template <BodyStructureConstraint MessageBody = boost::beast::http::string_body, class FieldsContainer = boost::beast::http::fields>
        class Response
        {
        private:
            /** @brief 被此类包装的 Boost.Beast 响应对象。 */
            boost::beast::http::response<MessageBody, FieldsContainer> MemberRes;

        public:
            /** @brief 封装的 Boost.Beast 响应类型。 */
            using UnderlyingContainer = boost::beast::http::response<MessageBody, FieldsContainer>;
            /** @brief 响应正文类型。 */
            using MessageBodyType = MessageBody;
            /** @brief 头部字段容器类型。 */
            using FieldsContainerType = FieldsContainer;

            /** @brief 创建默认响应。 */
            Response() = default;

            /**
             * @brief 创建指定状态码和版本的响应。
             * @param StatusValue HTTP 状态码。
             * @param VersionValue HTTP 版本号。
             */
            Response(boost::beast::http::status StatusValue,
                unsigned VersionValue)
                : MemberRes{StatusValue, VersionValue} {}

            /**
             * @brief 获取底层响应。
             * @return 底层响应的可写引用。
             */
            UnderlyingContainer &Base() { return MemberRes; }
            /**
             * @brief 以只读方式获取底层响应。
             * @return 底层响应的只读引用。
             */
            UnderlyingContainer const &Base() const { return MemberRes; }

            /**
             * @brief 获取 HTTP 响应状态码。
             * @return 当前状态码。
             */
            boost::beast::http::status Result() const { return MemberRes.result(); }
            /**
             * @brief 设置 HTTP 响应状态码。
             * @param StatusCode 要设置的状态码。
             */
            void Result(boost::beast::http::status StatusCode) { MemberRes.result(StatusCode); }
            /**
             * @brief 获取状态码的整数表示。
             * @return 状态码数值。
             */
            unsigned ResultInt() const { return MemberRes.result_int(); }
            /**
             * @brief 获取可修改的响应原因短语。
             * @return 原因短语的可写引用。
             */
            std::string &Reason() { return MemberRes.reason(); }
            /**
             * @brief 以只读方式获取响应原因短语。
             * @return 原因短语的只读引用。
             */
            std::string const &Reason() const { return MemberRes.reason(); }
            /**
             * @brief 设置 HTTP 响应原因短语。
             * @param ReasonValue 要设置的原因短语。
             */
            void Reason(boost::beast::string_view ReasonValue) { MemberRes.reason(ReasonValue); }

            /**
             * @brief 获取 HTTP 响应版本。
             * @return 当前版本号。
             */
            unsigned Version() const { return MemberRes.version(); }
            /**
             * @brief 设置 HTTP 响应版本。
             * @param VersionValue 要设置的版本号。
             */
            void Version(unsigned VersionValue) { MemberRes.version(VersionValue); }
            /**
             * @brief 获取响应的 Keep-Alive 状态。
             * @return 启用持久连接时为 `true`。
             */
            bool KeepAlive() const { return MemberRes.keep_alive(); }
            /**
             * @brief 设置响应的 Keep-Alive 状态。
             * @param KeepAliveValue 是否启用持久连接。
             */
            void KeepAlive(bool KeepAliveValue) { MemberRes.keep_alive(KeepAliveValue); }

            /**
             * @brief 设置 HTTP 响应头字段。
             * @param Key 头字段标识。
             * @param Value 字段值。
             */
            void Set(boost::beast::http::field Key, boost::beast::string_view Value) { MemberRes.set(Key, Value); }
            /**
             * @brief 获取 HTTP 响应头字段值。
             * @param Key 要查询的头字段标识。
             * @return 指向字段值的非拥有视图。
             */
            boost::beast::string_view At(boost::beast::http::field Key) const { return MemberRes.at(Key); }
            /**
             * @brief 移除 HTTP 响应头字段。
             * @param Key 要移除的头字段标识。
             */
            void Erase(boost::beast::http::field Key) { MemberRes.erase(Key); }

            /**
             * @brief 获取可修改的 HTTP 响应正文。
             * @return 响应正文的可写引用。
             */
            typename MessageBody::value_type &Body() { return MemberRes.body(); }
            /**
             * @brief 以只读方式获取 HTTP 响应正文。
             * @return 响应正文的只读引用。
             */
            typename MessageBody::value_type const &Body() const { return MemberRes.body(); }
            /**
             * @brief 获取响应正文长度上限。
             * @return 正文最大字节数。
             */
            std::uint64_t BodyLimit() const { return MemberRes.body_limit(); }
            /**
             * @brief 设置响应正文长度上限。
             * @param Length 正文最大字节数。
             */
            void BodyLimit(std::uint64_t Length) { MemberRes.body_limit(Length); }
            /**
             * @brief 准备 HTTP 响应正文并更新 `Content-Length` 头字段。
             */
            void PreparePayload() { MemberRes.prepare_payload(); }
            /**
             * @brief 检查响应是否包含 `Content-Length` 头字段。
             * @return 存在该字段时为 `true`。
             */
            bool HasContentLength() const { return MemberRes.has_content_length(); }
            /**
             * @brief 获取 `Content-Length` 头字段值。
             * @return 正文长度（字节）。
             */
            std::uint64_t ContentLength() const { return MemberRes.content_length(); }

            /**
             * @brief 将响应序列化为文本。
             * @return HTTP 响应文本。
             */
            std::string ToString() const
            {
                std::ostringstream Output;
                Output << MemberRes;
                return Output.str();
            }

            /**
             * @brief 从完整 HTTP 响应文本解析响应。
             * @param Data 待解析文本。
             * @return 输入恰好包含一个完整响应且解析成功时为 `true`。
             */
            bool FromString(std::string_view Data)
            {
                boost::beast::error_code Ec;
                boost::beast::http::response_parser<MessageBody> Parser;
                Parser.eager(true);
                Parser.header_limit(64 * 1024);
                Parser.body_limit(64 * 1024 * 1024);
                const auto Consumed = Parser.put(
                    boost::asio::buffer(Data.data(), Data.size()), Ec);
                if (Ec || !Parser.is_done() || Consumed != Data.size())
                    return false;
                MemberRes = Parser.get();
                return true;
            }
        }; // end class Response
    }
} // end namespace Protocol
