/**
 * @file Builtins.hpp
 * @brief 内置中间件
 * @details 提供日志、CORS、错误处理、JSON 解析等常用中间件。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include "Middleware.hpp"

#include <boost/beast.hpp>
#include <boost/json.hpp>

#include <chrono>
#include <cstdio>

namespace Wan::Web
{
    namespace Http = boost::beast::http;

    /**
     * @brief 创建记录请求方法、路径和处理耗时的中间件。
     * @return 日志中间件，默认优先级为 0。
     */
    inline auto LoggingMiddleware() -> Middleware
    {
        return Middleware{
            [](Context& Ctx, NextFn Next) -> Net::awaitable<void>
            {
                auto Start = std::chrono::steady_clock::now();
                co_await Next();
                auto End = std::chrono::steady_clock::now();
                auto Ms = std::chrono::duration_cast<
                    std::chrono::milliseconds>(End - Start);

                std::printf("[%s] %s -> %d (%lldms)\n",
                    std::string(Ctx.MethodString()).c_str(),
                    std::string(Ctx.Target()).c_str(),
                    static_cast<int>(Ctx.Response().result()),
                    static_cast<long long>(Ms.count()));
            },
            0,
            "logging"};
    }

    /**
     * @brief 创建设置 CORS 响应头的中间件。
     * @param AllowOrigin `Access-Control-Allow-Origin` 值。
     * @param AllowMethods `Access-Control-Allow-Methods` 值。
     * @param AllowHeaders `Access-Control-Allow-Headers` 值。
     * @return CORS 中间件，优先级为 10。
     */
    inline auto CorsMiddleware(
        std::string AllowOrigin = "*",
        std::string AllowMethods = "GET, POST, PUT, DELETE, PATCH, OPTIONS",
        std::string AllowHeaders = "Content-Type, Authorization")
        -> Middleware
    {
        return Middleware{
            [AllowOrigin, AllowMethods,
                AllowHeaders](Context& Ctx, NextFn Next)
                -> Net::awaitable<void>
            {
                auto& Resp = Ctx.Response();
                Resp.set(Http::field::access_control_allow_origin,
                    AllowOrigin);
                Resp.set(Http::field::access_control_allow_methods,
                    AllowMethods);
                Resp.set(Http::field::access_control_allow_headers,
                    AllowHeaders);

                if (Ctx.Method() == Http::verb::options)
                {
                    Ctx.Status(Http::status::no_content);
                    co_return;
                }

                co_await Next();
            },
            10,
            "cors"};
    }

    /**
     * @brief 创建将处理器异常转换为 HTTP 500 响应的中间件。
     * @return 错误处理中间件，优先级为 -100。
     */
    inline auto ErrorHandlerMiddleware() -> Middleware
    {
        return Middleware{
            [](Context& Ctx, NextFn Next) -> Net::awaitable<void>
            {
                try
                {
                    co_await Next();
                }
                catch (const std::exception& E)
                {
                    Ctx.Status(Http::status::internal_server_error);
                    boost::json::object Error;
                    Error["error"] = "internal_server_error";
                    Error["message"] = E.what();
                    Ctx.Json(boost::json::serialize(Error));
                }
                catch (...)
                {
                    Ctx.Status(Http::status::internal_server_error);
                    Ctx.Json(R"({"error":"internal_server_error"})");
                }
            },
            -100,
            "error_handler"};
    }
}
