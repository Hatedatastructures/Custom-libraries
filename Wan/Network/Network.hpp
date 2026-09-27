/**
 * @file Network.hpp
 * @brief 网络模块入口
 * @details 提供网络协议、加密等功能
 * @note 会话管理已迁移至 Wan/Web 模块
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

// 加密模块
#include "./Crypt/Encryption.hpp"

// 协议模块
#include "./Agreement/Http.hpp"
#include "./Agreement/Json.hpp"
#include "./Agreement/Assist.hpp"
#include "./Agreement/Protocol.hpp"
#include "./Agreement/Conversion.hpp"

namespace Wan
{
    /**
     * @brief 网络模块
     * @note 提供网络协议、加密等功能
     * @warning 会话管理已迁移至 Wan::Web 模块，请使用 Wan/Web/Web.hpp
     */
    namespace Network
    {
        /**
         * @brief 协议模块
         * @note 提供 TCP 协议的定义、转换、校验等功能
         */
        namespace Agreement
        {
            /** @brief 协议 JSON 封装。 */
            using Protocol::Json;
            /** @brief 协议请求类型。 */
            using Protocol::Request;
            /** @brief 协议响应类型。 */
            using Protocol::Response;
            /** @brief 请求头类型。 */
            using Protocol::RequestHeader;
            /** @brief 响应头类型。 */
            using Protocol::ResponseHeader;

            /** @brief 数据校验算法类型。 */
            using Protocol::Assist::ChecksumType;
            /** @brief 协议类别。 */
            using Protocol::Assist::ProtocolType;
            /** @brief 协议头基类。 */
            using Protocol::Assist::ProtocolHeader;

            /** @brief 协议对象与 JSON 之间的转换器。 */
            using Protocol::Conversion::ProtocolConverter;
        }

        /**
         * @brief HTTP 模块
         * @note 提供 HTTP 协议的封装等功能
         */
        namespace Http
        {
            using namespace Protocol::Http;
        }

        /**
         * @brief 加密模块
         * @note 提供加密、解密、哈希等功能
         */
        namespace Ciphertext
        {
            using namespace Encryption;
        }
    }
}
