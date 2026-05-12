/**
 * @file builtins.hpp
 * @brief 内置中间件
 * @details 提供日志、CORS、错误处理等常用中间件。
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <functional>
#include <iostream>
#include <string>

#include <wan/web/core/context.hpp>
#include <wan/web/middleware/middleware.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    namespace http = boost::beast::http;

    /**
     * @brief 日志中间件
     * @details 打印请求方法和路径。
     */
    [[nodiscard]] inline auto logger() -> middleware
    {
        return [](context& ctx, std::function<net::awaitable<void>()> next) -> net::awaitable<void>
        {
            std::cout << "[wan/web] " << ctx.method_string() << " " << ctx.target() << std::endl;
            co_await next();
        };
    }

    /**
     * @brief CORS 中间件
     * @details 设置 CORS 头部。
     */
    [[nodiscard]] inline auto cors() -> middleware
    {
        return [](context& ctx, std::function<net::awaitable<void>()> next) -> net::awaitable<void>
        {
            ctx.set("Access-Control-Allow-Origin", "*");
            ctx.set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            ctx.set("Access-Control-Allow-Headers", "Content-Type");

            // OPTIONS 请求直接返回
            if (ctx.method() == http::verb::options)
            {
                ctx.status(http::status::no_content);
                co_return;
            }

            co_await next();
        };
    }

    /**
     * @brief 错误处理中间件
     * @details 捕获异常并返回 500。
     */
    [[nodiscard]] inline auto error_handler() -> middleware
    {
        return [](context& ctx, std::function<net::awaitable<void>()> next) -> net::awaitable<void>
        {
            try
            {
                co_await next();
            }
            catch (const std::exception& e)
            {
                ctx.status(http::status::internal_server_error)
                    .set("Content-Type", "application/json")
                    .json("{\"error\": \"" + std::string(e.what()) + "\"}");
            }
            catch (...)
            {
                ctx.status(http::status::internal_server_error)
                    .text("Internal Server Error");
            }
        };
    }

    /**
     * @brief JSON 解析中间件
     * @details 自动解析 JSON 请求体。
     */
    [[nodiscard]] inline auto json_parser() -> middleware
    {
        return [](context& ctx, std::function<net::awaitable<void>()> next) -> net::awaitable<void>
        {
            auto content_type = ctx.header("Content-Type");
            if (content_type.find("application/json") != std::string_view::npos)
            {
                // JSON 解析已在 context.json() 中处理
            }
            co_await next();
        };
    }
}