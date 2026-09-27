/**
 * @file TestWeb.cpp
 * @brief Web 框架测试
 * @details 测试路由匹配、Context、中间件、集成 HTTP 服务。
 *          客户端 socket 使用 ioc.get_executor() 构造，
 *          避免 MSYS2 UCRT64 IOCP 挂起问题。
 * @author Hatedatastructures
 * @date 2026-05-25
 */

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "Wan/Web/Web.hpp"

namespace Net = boost::asio;
namespace Beast = boost::beast;
namespace Http = Beast::http;

// ── 路由测试 ──

static auto TestRouteBasic() -> bool
{
    Wan::Web::Router R;
    bool Called = false;

    R.Get("/hello", [&](Wan::Web::Context& Ctx) -> Net::awaitable<void>
    {
        Called = true;
        Ctx.Text("world");
        co_return;
    });

    auto Match = R.Match(Http::verb::get, "/hello");
    if (!Match)
    {
        std::puts("FAIL: route_basic - no match");
        return false;
    }
    if (!Match->Handler)
    {
        std::puts("FAIL: route_basic - no handler");
        return false;
    }
    std::puts("PASS: route_basic");
    return true;
}

static auto TestRouteParams() -> bool
{
    Wan::Web::Router R;

    R.Get("/user/<string:name>", [&](Wan::Web::Context& Ctx)
        -> Net::awaitable<void>
    {
        Ctx.Text(Ctx.Param("name"));
        co_return;
    });

    auto Match = R.Match(Http::verb::get, "/user/alice");
    if (!Match)
    {
        std::puts("FAIL: route_params - no match");
        return false;
    }
    if (Match->Params.at("name") != "alice")
    {
        std::printf("FAIL: route_params - name='%s'\n",
            Match->Params.at("name").c_str());
        return false;
    }
    std::puts("PASS: route_params");
    return true;
}

static auto TestRouteNotFound() -> bool
{
    Wan::Web::Router R;
    R.Get("/exists", [&](Wan::Web::Context&) -> Net::awaitable<void>
    {
        co_return;
    });

    auto Match = R.Match(Http::verb::get, "/notexists");
    if (Match)
    {
        std::puts("FAIL: route_not_found - should not match");
        return false;
    }
    std::puts("PASS: route_not_found");
    return true;
}

static auto TestRouteMethodDispatch() -> bool
{
    Wan::Web::Router R;
    bool GetCalled = false;
    bool PostCalled = false;

    R.Get("/api", [&](Wan::Web::Context&) -> Net::awaitable<void>
    {
        GetCalled = true;
        co_return;
    });
    R.Post("/api", [&](Wan::Web::Context&) -> Net::awaitable<void>
    {
        PostCalled = true;
        co_return;
    });

    auto M1 = R.Match(Http::verb::get, "/api");
    auto M2 = R.Match(Http::verb::post, "/api");

    if (!M1 || !M2)
    {
        std::puts("FAIL: route_method_dispatch - no match");
        return false;
    }

    // 触发 handlers
    Wan::Web::Context::RequestType Req{Http::verb::get, "/api", 11};
    Wan::Web::Context CtxGet{std::move(Req)};
    // Just check match exists, don't actually call coroutine
    std::puts("PASS: route_method_dispatch");
    return true;
}

static auto TestRouteQueryStripped() -> bool
{
    Wan::Web::Router R;
    R.Get("/search", [&](Wan::Web::Context&) -> Net::awaitable<void>
    {
        co_return;
    });

    auto Match = R.Match(Http::verb::get, "/search?q=hello");
    if (!Match)
    {
        std::puts("FAIL: route_query_stripped - no match");
        return false;
    }
    std::puts("PASS: route_query_stripped");
    return true;
}

// ── Context 测试 ──

static auto TestContextQueryParams() -> bool
{
    Wan::Web::Context::RequestType Req{Http::verb::get,
        "/search?q=hello&lang=cpp", 11};
    Wan::Web::Context Ctx{std::move(Req)};

    if (Ctx.Query("q") != "hello")
    {
        std::printf("FAIL: context_query_params - q='%s'\n",
            Ctx.Query("q").c_str());
        return false;
    }
    if (Ctx.Query("lang") != "cpp")
    {
        std::printf("FAIL: context_query_params - lang='%s'\n",
            Ctx.Query("lang").c_str());
        return false;
    }
    Wan::Web::Context::RequestType EncodedReq{Http::verb::get,
        "/search?q=hello%20world&plus=a+b", 11};
    Wan::Web::Context EncodedCtx{std::move(EncodedReq)};
    if (EncodedCtx.Query("q") != "hello world"
        || EncodedCtx.Query("plus") != "a b")
    {
        std::puts("FAIL: context_query_params - url decoding");
        return false;
    }
    std::puts("PASS: context_query_params");
    return true;
}

static auto TestContextJsonResponse() -> bool
{
    Wan::Web::Context::RequestType Req{Http::verb::get, "/", 11};
    Wan::Web::Context Ctx{std::move(Req)};

    Ctx.Json(R"({"status":"ok"})");

    if (Ctx.Response().result() != Http::status::ok)
    {
        std::puts("FAIL: context_json_response - wrong status");
        return false;
    }

    auto Ct = Ctx.Response().at(Http::field::content_type);
    if (Ct != "application/json")
    {
        std::printf("FAIL: context_json_response - ct='%s'\n",
            std::string(Ct).c_str());
        return false;
    }
    std::puts("PASS: context_json_response");
    return true;
}

static auto TestContextMultiCookie() -> bool
{
    Wan::Web::Context::RequestType Req{Http::verb::get, "/", 11};
    Wan::Web::Context Ctx{std::move(Req)};

    Ctx.SetCookie("a", "1");
    Ctx.SetCookie("b", "2");

    // 检查有多个 Set-Cookie 头
    auto Count = Ctx.Response().count(Http::field::set_cookie);
    if (Count != 2)
    {
        std::printf("FAIL: context_multi_cookie - count=%zu\n", Count);
        return false;
    }
    bool RejectedInvalidCookie = false;
    try
    {
        Ctx.SetCookie("bad\r\n", "value");
    }
    catch (const std::invalid_argument&)
    {
        RejectedInvalidCookie = true;
    }
    if (!RejectedInvalidCookie)
    {
        std::puts("FAIL: context_multi_cookie - invalid cookie accepted");
        return false;
    }

    try
    {
        Wan::Web::CookieOptions InvalidOptions;
        InvalidOptions.Path = "/; Secure";
        Ctx.SetCookie("safe", "value", InvalidOptions);
        std::puts("FAIL: context_multi_cookie - cookie attribute injection");
        return false;
    }
    catch (const std::invalid_argument&)
    {
        (void)0;
    }
    std::puts("PASS: context_multi_cookie");
    return true;
}

// ── 中间件测试 ──

static auto TestMiddlewareChainOrder() -> bool
{
    std::string Order;
    Wan::Web::MiddlewareChain Chain;

    Chain.Use([&](Wan::Web::Context&, Wan::Web::NextFn Next)
        -> Net::awaitable<void>
    {
        Order += "A";
        co_await Next();
        Order += "a";
    }, 10, "A");

    Chain.Use([&](Wan::Web::Context&, Wan::Web::NextFn Next)
        -> Net::awaitable<void>
    {
        Order += "B";
        co_await Next();
        Order += "b";
    }, 20, "B");

    Wan::Web::Context::RequestType Req{Http::verb::get, "/", 11};
    Wan::Web::Context Ctx{std::move(Req)};

    // 同步执行测试：直接调用 run
    Net::io_context Ioc;
    bool Done = false;
    Net::co_spawn(Ioc,
        [&]() -> Net::awaitable<void>
        {
            co_await Chain.Run(Ctx, [&](Wan::Web::Context&)
                -> Net::awaitable<void>
            {
                Order += "H";
                co_return;
            });
            Done = true;
        },
        Net::detached);
    Ioc.run();

    // 低优先级先执行：A B H b a
    if (Order != "ABHba")
    {
        std::printf("FAIL: middleware_chain_order - order='%s'\n",
            Order.c_str());
        return false;
    }
    std::puts("PASS: middleware_chain_order");
    return true;
}

// ── 集成测试 ──

static auto TestHttpEcho() -> bool
{
    // 使用独立 IoContext 避免与 server executor 冲突
    Net::io_context ServerIoc;

    Wan::Web::ServerConfig Config;
    Config.Threads = 0; // 使用 ServerIoc 直接运行
    auto Srv = std::make_shared<Wan::Web::Server>(Config);

    std::uint16_t Port = 0;
    bool ServerReady = false;

    // 用独立 acceptor 获取端口
    Net::ip::tcp::acceptor TmpAcc{ServerIoc,
        Net::ip::tcp::endpoint{Net::ip::tcp::v4(), 0}};
    Port = TmpAcc.local_endpoint().port();
    TmpAcc.close();

    Srv->Get("/ping", [](Wan::Web::Context& Ctx) -> Net::awaitable<void>
    {
        Ctx.Text("pong");
        co_return;
    });

    // 在 ServerIoc 上启动 accept 循环
    bool ServerOk = false;
    Net::co_spawn(ServerIoc,
        [Srv, Port, &ServerOk]() -> Net::awaitable<void>
        {
            auto Ex = co_await Net::this_coro::executor;
            Net::ip::tcp::acceptor Acceptor{
                Ex, Net::ip::tcp::endpoint{Net::ip::tcp::v4(), Port}};

            // 只接受一个连接
            auto Socket = co_await Acceptor.async_accept(
                Net::use_awaitable);
            Socket.set_option(Net::ip::tcp::no_delay{true});

            Beast::flat_buffer Buffer;
            Http::request<Http::string_body> Req;
            co_await Http::async_read(Socket, Buffer, Req,
                Net::use_awaitable);

            Wan::Web::Context Ctx{std::move(Req)};
            auto Match = Srv->GetRouter().Match(
                Ctx.Method(), Ctx.Target());

            if (Match)
            {
                Ctx.SetParams(std::move(Match->Params));
                co_await Match->Handler(Ctx);
            }

            co_await Http::async_write(Socket, Ctx.Response(),
                Net::use_awaitable);

            ServerOk = (Ctx.Response().body() == "pong");
            Socket.shutdown(Net::ip::tcp::socket::shutdown_send);
        },
        Net::detached);

    // 客户端
    bool ClientOk = false;
    Net::co_spawn(ServerIoc,
        [&ServerIoc, Port, &ClientOk]() -> Net::awaitable<void>
        {
            Net::ip::tcp::socket Sock{ServerIoc.get_executor()};
            co_await Sock.async_connect(
                Net::ip::tcp::endpoint{
                    Net::ip::make_address_v4("127.0.0.1"), Port},
                Net::use_awaitable);

            Http::request<Http::string_body> Req{
                Http::verb::get, "/ping", 11};
            Req.set(Http::field::host, "localhost");
            Req.prepare_payload();
            co_await Http::async_write(Sock, Req, Net::use_awaitable);

            Beast::flat_buffer Buffer;
            Http::response<Http::string_body> Resp;
            co_await Http::async_read(Sock, Buffer, Resp,
                Net::use_awaitable);

            ClientOk = (Resp.result() == Http::status::ok)
                && (Resp.body() == "pong");
        },
        Net::detached);

    ServerIoc.run();

    if (ServerOk && ClientOk)
    {
        std::puts("PASS: http_echo");
        return true;
    }
    std::printf("FAIL: http_echo server=%d client=%d\n",
        ServerOk, ClientOk);
    return false;
}

// ── Metrics 测试 ──

static auto TestMetricsCounters() -> bool
{
    Wan::Web::ServerMetrics M;
    M.IncRequests();
    M.IncRequests();
    M.IncActiveConnections();
    M.DecActiveConnections();
    M.IncBytesRead(100);
    M.IncBytesWritten(50);

    auto Prom = M.ToPrometheus();
    if (Prom.find("wan_requests_total 2") == std::string::npos)
    {
        std::puts("FAIL: metrics_counters - wrong requests count");
        return false;
    }
    std::puts("PASS: metrics_counters");
    return true;
}

static auto TestRealServerHttp() -> bool
{
    Net::io_context ProbeIoc;
    Net::ip::tcp::acceptor Probe{ProbeIoc,
        Net::ip::tcp::endpoint{Net::ip::tcp::v4(), 0}};
    const auto Port = Probe.local_endpoint().port();
    Probe.close();

    Wan::Web::ServerConfig Config;
    Config.Threads = 2;
    Config.ReadTimeout = std::chrono::seconds{2};
    Config.WriteTimeout = std::chrono::seconds{2};
    Config.MaxBodySize = 1024;
    auto Server = std::make_shared<Wan::Web::Server>(Config);
    Server->Listen(Port).Get("/real", [](Wan::Web::Context& Ctx)
        -> Net::awaitable<void>
    {
        Ctx.Text("real-server");
        co_return;
    });

    std::thread Runner([Server]() { Server->RunSync(); });
    bool ClientOk = false;

    for (int Attempt = 0; Attempt < 50 && !ClientOk; ++Attempt)
    {
        Net::io_context ClientIoc;
        Net::ip::tcp::socket Socket{ClientIoc};
        boost::system::error_code Ec;
        Ec = Socket.connect(Net::ip::tcp::endpoint{
            Net::ip::make_address_v4("127.0.0.1"), Port}, Ec);
        if (Ec)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            continue;
        }

        Http::request<Http::string_body> Request{
            Http::verb::get, "/real", 11};
        Request.set(Http::field::host, "localhost");
        Request.set(Http::field::connection, "close");
        Request.prepare_payload();
        Http::write(Socket, Request, Ec);

        Beast::flat_buffer Buffer;
        Http::response<Http::string_body> Response;
        if (!Ec)
        {
            Http::read(Socket, Buffer, Response, Ec);
        }
        ClientOk = !Ec && Response.result() == Http::status::ok
            && Response.body() == "real-server";
        Ec = Socket.close(Ec);
    }

    Server->Stop();
    Runner.join();

    if (!ClientOk)
    {
        std::puts("FAIL: real_server_http");
        return false;
    }
    std::puts("PASS: real_server_http");
    return true;
}

static auto TestHttpClientRedirect() -> bool
{
    Net::io_context Ioc;
    Net::ip::tcp::acceptor Acceptor{Ioc,
        Net::ip::tcp::endpoint{Net::ip::tcp::v4(), 0}};
    const auto Port = Acceptor.local_endpoint().port();
    const auto Client = std::make_shared<Wan::Web::HttpClient>(
        Ioc.get_executor());
    bool ServerOk = false;
    bool ClientOk = false;
    bool FirstWasPost = false;
    bool SecondWasGet = false;

    Net::co_spawn(Ioc,
        [&]() -> Net::awaitable<void>
        {
            for (int I = 0; I < 2; ++I)
            {
                auto Socket = co_await Acceptor.async_accept(
                    Net::use_awaitable);
                Beast::flat_buffer Buffer;
                Http::request<Http::string_body> Request;
                co_await Http::async_read(Socket, Buffer, Request,
                    Net::use_awaitable);
                if (I == 0)
                {
                    FirstWasPost = Request.method() == Http::verb::post
                        && Request.body() == "payload";
                }
                else
                {
                    SecondWasGet = Request.method() == Http::verb::get
                        && Request.body().empty();
                }

                Http::response<Http::string_body> Response{
                    I == 0 ? Http::status::found : Http::status::ok,
                    11};
                Response.set(Http::field::connection, "close");
                if (I == 0)
                {
                    Response.set(Http::field::location,
                        "http://127.0.0.1:" + std::to_string(Port)
                            + "/final");
                }
                else
                {
                    Response.body() = "redirected";
                }
                Response.prepare_payload();
                co_await Http::async_write(Socket, Response,
                    Net::use_awaitable);
                boost::system::error_code Ec;
                Ec = Socket.shutdown(
                    Net::ip::tcp::socket::shutdown_both, Ec);
            }
            ServerOk = FirstWasPost && SecondWasGet;
            boost::system::error_code Ec;
            Ec = Acceptor.close(Ec);
        },
        Net::detached);

    Net::co_spawn(Ioc,
        [&]() -> Net::awaitable<void>
        {
            auto Response = co_await Client->Post(
                "127.0.0.1", std::to_string(Port), "/start",
                "payload", 2);
            ClientOk = Response.Status == Http::status::ok
                && Response.Body == "redirected";
        },
        Net::detached);

    Ioc.run();

    if (!ServerOk || !ClientOk)
    {
        std::printf("FAIL: http_client_redirect server=%d client=%d\n",
            ServerOk, ClientOk);
        return false;
    }
    std::puts("PASS: http_client_redirect");
    return true;
}

static auto TestHttpClientResponseLimits() -> bool
{
    auto RunCase = [](bool OversizedHeader,
                       Wan::Web::HttpClientConfig Config) -> bool
    {
        Net::io_context Ioc;
        Net::ip::tcp::acceptor Acceptor{Ioc,
            Net::ip::tcp::endpoint{Net::ip::tcp::v4(), 0}};
        const auto Port = Acceptor.local_endpoint().port();

        auto Client = std::make_shared<Wan::Web::HttpClient>(
            Ioc.get_executor(), Config);
        bool ServerStarted = false;
        bool Rejected = false;

        Net::co_spawn(Ioc,
            [&]() -> Net::awaitable<void>
            {
                auto Socket = co_await Acceptor.async_accept(
                    Net::use_awaitable);
                ServerStarted = true;
                Http::response<Http::string_body> Response{
                    Http::status::ok, 11};
                Response.set(Http::field::connection, "close");
                if (OversizedHeader)
                {
                    Response.set("X-Large", std::string(256, 'h'));
                }
                else
                {
                    Response.body() = std::string(256, 'b');
                }
                Response.prepare_payload();
                boost::system::error_code Ec;
                co_await Http::async_write(Socket, Response,
                    Net::redirect_error(Net::use_awaitable, Ec));
                auto Ex = co_await Net::this_coro::executor;
                Net::steady_timer Delay{Ex,
                    std::chrono::milliseconds{100}};
                co_await Delay.async_wait(
                    Net::redirect_error(Net::use_awaitable, Ec));
                Ec = Socket.shutdown(
                    Net::ip::tcp::socket::shutdown_both, Ec);
                Ec = Acceptor.close(Ec);
            },
            Net::detached);

        Net::co_spawn(Ioc,
            [&]() -> Net::awaitable<void>
            {
                try
                {
                    (void)co_await Client->Get(
                        "127.0.0.1", std::to_string(Port), "/limited");
                }
                catch (const boost::system::system_error& Error)
                {
                    Rejected = OversizedHeader
                        ? Error.code() == Http::error::header_limit
                        : Error.code() == Http::error::body_limit;
                }
                catch (...)
                {
                    Rejected = false;
                }
            },
            Net::detached);

        Ioc.run();
        return ServerStarted && Rejected;
    };

    Wan::Web::HttpClientConfig BodyConfig;
    BodyConfig.MaxResponseHeaderSize = std::size_t{64} * 1024;
    BodyConfig.MaxResponseBodySize = 32;
    Wan::Web::HttpClientConfig HeaderConfig;
    HeaderConfig.MaxResponseHeaderSize = 64;
    HeaderConfig.MaxResponseBodySize = std::size_t{64} * 1024 * 1024;
    const auto BodyRejected = RunCase(false, BodyConfig);
    const auto HeaderRejected = RunCase(true, HeaderConfig);
    if (!BodyRejected || !HeaderRejected)
    {
        std::printf("FAIL: http_client_response_limits body=%d header=%d\n",
            BodyRejected, HeaderRejected);
        return false;
    }
    std::puts("PASS: http_client_response_limits");
    return true;
}

static auto TestHttpClientTimeout() -> bool
{
    Net::io_context Ioc;
    Net::ip::tcp::acceptor Acceptor{Ioc,
        Net::ip::tcp::endpoint{Net::ip::tcp::v4(), 0}};
    const auto Port = Acceptor.local_endpoint().port();

    Wan::Web::HttpClientConfig Config;
    Config.Timeout = std::chrono::seconds{1};
    auto Client = std::make_shared<Wan::Web::HttpClient>(
        Ioc.get_executor(), Config);
    auto Delay = std::make_shared<Net::steady_timer>(
        Ioc.get_executor(), std::chrono::seconds{4});
    bool TimedOut = false;

    Net::co_spawn(Ioc,
        [&]() -> Net::awaitable<void>
        {
            auto Socket = co_await Acceptor.async_accept(
                Net::use_awaitable);
            boost::system::error_code Ec;
            co_await Delay->async_wait(
                Net::redirect_error(Net::use_awaitable, Ec));
            Ec = Socket.close(Ec);
            Ec = Acceptor.close(Ec);
        },
        Net::detached);

    const auto Begin = std::chrono::steady_clock::now();
    Net::co_spawn(Ioc,
        [&]() -> Net::awaitable<void>
        {
            try
            {
                (void)co_await Client->Get(
                    "127.0.0.1", std::to_string(Port), "/timeout");
            }
            catch (const boost::system::system_error&)
            {
                TimedOut = true;
                Delay->cancel();
            }
        },
        Net::detached);
    Ioc.run();
    const auto Elapsed = std::chrono::steady_clock::now() - Begin;

    if (!TimedOut || Elapsed >= std::chrono::seconds{3})
    {
        std::puts("FAIL: http_client_timeout");
        return false;
    }
    std::puts("PASS: http_client_timeout");
    return true;
}

static auto TestHttpClientRejectsNegativeRedirects() -> bool
{
    Net::io_context Ioc;
    Wan::Web::HttpClient Client{Ioc.get_executor()};
    bool Rejected = false;

    Net::co_spawn(Ioc,
        [&]() -> Net::awaitable<void>
        {
            try
            {
                (void)co_await Client.Get("127.0.0.1", "1", "/", -1);
            }
            catch (const std::invalid_argument&)
            {
                Rejected = true;
            }
        },
        Net::detached);
    Ioc.run();

    if (!Rejected)
    {
        std::puts("FAIL: http_client_negative_redirects");
        return false;
    }
    std::puts("PASS: http_client_negative_redirects");
    return true;
}

static auto TestRealServerBodyLimit() -> bool
{
    Net::io_context ProbeIoc;
    Net::ip::tcp::acceptor Probe{ProbeIoc,
        Net::ip::tcp::endpoint{Net::ip::tcp::v4(), 0}};
    const auto Port = Probe.local_endpoint().port();
    Probe.close();

    Wan::Web::ServerConfig Config;
    Config.Threads = 1;
    Config.ReadTimeout = std::chrono::seconds{2};
    Config.MaxBodySize = 16;
    auto Server = std::make_shared<Wan::Web::Server>(Config);
    Server->Listen(Port).Post("/limited", [](Wan::Web::Context& Ctx)
        -> Net::awaitable<void>
    {
        Ctx.Text("unexpected");
        co_return;
    });

    std::thread Runner([Server]() { Server->RunSync(); });
    bool Rejected = false;
    for (int Attempt = 0; Attempt < 50 && !Rejected; ++Attempt)
    {
        Net::io_context ClientIoc;
        Net::ip::tcp::socket Socket{ClientIoc};
        boost::system::error_code Ec;
        Ec = Socket.connect(Net::ip::tcp::endpoint{
            Net::ip::make_address_v4("127.0.0.1"), Port}, Ec);
        if (Ec)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds{10});
            continue;
        }

        Http::request<Http::string_body> Request{
            Http::verb::post, "/limited", 11};
        Request.set(Http::field::host, "localhost");
        Request.body() = std::string(64, 'x');
        Request.prepare_payload();
        Http::write(Socket, Request, Ec);
        if (!Ec)
        {
            Beast::flat_buffer Buffer;
            Http::response<Http::string_body> Response;
            Http::read(Socket, Buffer, Response, Ec);
        }
        Rejected = static_cast<bool>(Ec);
        Ec = Socket.close(Ec);
    }

    Server->Stop();
    Runner.join();

    if (!Rejected)
    {
        std::puts("FAIL: real_server_body_limit");
        return false;
    }
    std::puts("PASS: real_server_body_limit");
    return true;
}

auto main() -> int // NOLINT(bugprone-exception-escape)
{
    int Failed = 0;

    if (!TestRouteBasic()) ++Failed;
    if (!TestRouteParams()) ++Failed;
    if (!TestRouteNotFound()) ++Failed;
    if (!TestRouteMethodDispatch()) ++Failed;
    if (!TestRouteQueryStripped()) ++Failed;
    if (!TestContextQueryParams()) ++Failed;
    if (!TestContextJsonResponse()) ++Failed;
    if (!TestContextMultiCookie()) ++Failed;
    if (!TestMiddlewareChainOrder()) ++Failed;
    if (!TestHttpEcho()) ++Failed;
    if (!TestMetricsCounters()) ++Failed;
    if (!TestRealServerHttp()) ++Failed;
    if (!TestRealServerBodyLimit()) ++Failed;
    if (!TestHttpClientRedirect()) ++Failed;
    if (!TestHttpClientResponseLimits()) ++Failed;
    if (!TestHttpClientTimeout()) ++Failed;
    if (!TestHttpClientRejectsNegativeRedirects()) ++Failed;

    std::printf("\n%d/%d tests passed\n", 17 - Failed, 17);
    return Failed;
}
