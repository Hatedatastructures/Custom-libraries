/**
 * @file network.hpp
 * @brief 网络模块入口
 * @details 提供网络协议、加密等功能
 * @note 会话管理已迁移至 wan/web 模块
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

// 加密模块
#include "./crypt/encryption.hpp"

// 协议模块
#include "./agreement/http.hpp"
#include "./agreement/json.hpp"
#include "./agreement/assist.hpp"
#include "./agreement/protocol.hpp"
#include "./agreement/conversion.hpp"

namespace wan
{
    /**
     * @brief 网络模块
     * @note 提供网络协议、加密等功能
     * @warning 会话管理已迁移至 wan::web 模块，请使用 wan/web/web.hpp
     */
    namespace network
    {
        /**
         * @brief 协议模块
         * @note 提供 TCP 协议的定义、转换、校验等功能
         */
        namespace agreement
        {
            using protocol::json;
            using protocol::request;
            using protocol::response;
            using protocol::request_header;
            using protocol::response_header;

            using protocol::assist::checksum_type;
            using protocol::assist::protocol_type;
            using protocol::assist::protocol_header;

            using protocol::conversion::protocol_converter;
        }

        /**
         * @brief HTTP 模块
         * @note 提供 HTTP 协议的封装等功能
         */
        namespace http
        {
            using namespace protocol::http;
        }

        /**
         * @brief 加密模块
         * @note 提供加密、解密、哈希等功能
         */
        namespace ciphertext
        {
            using namespace encryption;
        }
    }
}