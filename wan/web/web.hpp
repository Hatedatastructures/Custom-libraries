/**
 * @file web.hpp
 * @brief Web 模块入口
 * @details 高性能协程驱动的 Web 框架，提供服务器、客户端、路由、中间件等组件。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

// 核心层
#include "./core/executor.hpp"
#include "./core/context.hpp"
#include "./core/metrics.hpp"

// 协议层
#include "./protocol/request.hpp"
#include "./protocol/response.hpp"
#include "./protocol/websocket.hpp"
#include "./protocol/http2.hpp"
#include "./protocol/hpack.hpp"

// 传输层
#include "./transport/connection.hpp"
#include "./transport/pool.hpp"
#include "./transport/ssl.hpp"
#include "./transport/limits.hpp"

// 路由层
#include "./router/router.hpp"

// 中间件层
#include "./middleware/middleware.hpp"
#include "./middleware/builtins.hpp"

// 服务层
#include "./server/server.hpp"
#include "./server/static.hpp"

// 客户端层
#include "./client/client.hpp"
#include "./client/proxy.hpp"

// Session 层
#include "./session/session.hpp"
#include "./session/store.hpp"

// 错误码
#include "./fault.hpp"

namespace wan::web
{
    /**
     * @brief Web 模块
     * @details 高性能协程驱动的 Web 框架
     * @note 使用示例：
     *   auto app = wan::web::make_server();
     *   app->get("/hello", [](context& ctx) -> net::awaitable<void> {
     *       ctx.text("Hello World");
     *       co_return;
     *   });
     *   app->use(wan::web::make_static("./public"));
     *   app->listen(8080);
     *   co_await app->run();
     */
}