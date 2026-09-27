/**
 * @file Conversion.hpp
 * @brief 协议转换定义
 * @details 提供协议对象与`Json`之间的互相转换
 */
#pragma once
#include <boost/json.hpp>
#include "./Protocol.hpp"
#include "./Json.hpp"
#include <optional>

namespace Protocol
{
    namespace Conversion
    {
    } // end namespace Conversion
} // end namespace Protocol

namespace Protocol::Conversion
{
    /**
     * @brief 协议转换器
     * @details 提供协议对象与`Json`之间的转换功能
     */
    class ProtocolConverter
    {
    public:
        /**
         * @brief 请求转`Json`
         * @tparam HeaderT 头部类型
         * @param RequestValue 请求对象
         * @return `Json`对象
         */
        template <HeaderConstraint HeaderT = RequestHeader>
        static Json RequestToJson(const Request<HeaderT> &RequestValue)
        {
            return RequestValue.ToJson();
        }
        /**
         * @brief `Json`转请求
         * @tparam HeaderT 头部类型
         * @param JsonObject `Json`对象
         * @return 请求对象
         */
        template <HeaderConstraint HeaderT = RequestHeader>
        static std::optional<Request<HeaderT>> JsonToRequest(const Json &JsonObject)
        {
            Request<HeaderT> RequestValue;
            if (RequestValue.FromJson(JsonObject))
                return RequestValue;
            return std::nullopt;
        }
        /**
         * @brief 响应转`Json`
         * @tparam HeaderT 头部类型
         * @param ResponseValue 响应对象
         * @return `Json`对象
         */
        template <HeaderConstraint HeaderT = ResponseHeader>
        static Json ResponseToJson(const Response<HeaderT> &ResponseValue)
        {
            return ResponseValue.ToJson();
        }
        /**
         * @brief `Json`转响应
         * @tparam HeaderT 头部类型
         * @param JsonObject `Json`对象
         * @return 响应对象
         */
        template <HeaderConstraint HeaderT = ResponseHeader>
        static std::optional<Response<HeaderT>> JsonToResponse(const Json &JsonObject)
        {
            Response<HeaderT> ResponseValue;
            if (ResponseValue.FromJson(JsonObject))
                return ResponseValue;
            return std::nullopt;
        }
    }; // end class ProtocolConverter
} // end namespace Conversion
