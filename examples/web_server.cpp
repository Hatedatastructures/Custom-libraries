/**
 * @file web_server.cpp
 * @brief Web 服务器示例
 * @details 展示 wan::web::server 的基本用法。
 * @author Hatedatastructures
 * @date 2026-05-11
 */

#include <Wan.hpp>

using namespace wan::web;
namespace http = boost::beast::http;

net::awaitable<void> hello_handler(context& ctx)
{
    ctx.text("Hello World");
    co_return;
}

net::awaitable<void> json_handler(context& ctx)
{
    ctx.json("{\"message\": \"Hello JSON\"}");
    co_return;
}

net::awaitable<void> user_handler(context& ctx)
{
    auto id = ctx.param<int>("id");
    if (id)
    {
        ctx.json("{\"user_id\": " + std::to_string(*id) + "}");
    }
    else
    {
        ctx.status(http::status::bad_request).text("Invalid user ID");
    }
    co_return;
}

int main()
{
    auto app = make_server();

    // 注册路由
    app->get("/", hello_handler)
        .get("/json", json_handler)
        .get("/users/<int:id>", user_handler);

    // 注册中间件
    app->use(logger());

    // 设置端口
    app->listen(8080);
    std::cout << "[wan/web] Server running on http://localhost:8080" << std::endl;

    // 运行服务器（server 内部管理 io_context）
    net::io_context ioc;
    net::co_spawn(ioc, app->run(), net::detached);
    ioc.run();

    return 0;
}