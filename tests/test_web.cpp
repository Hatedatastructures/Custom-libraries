/**
 * @file test_web.cpp
 * @brief wan/web 模块测试
 * @author Hatedatastructures
 * @date 2026-05-12
 */

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include <wan/web/web.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;

    // === 测试框架 ===

    static int tests_passed = 0;
    static int tests_failed = 0;

    auto test(const char* name, bool condition) -> void
    {
        if (condition)
        {
            std::cout << "[PASS] " << name << std::endl;
            ++tests_passed;
        }
        else
        {
            std::cout << "[FAIL] " << name << std::endl;
            ++tests_failed;
        }
    }

    auto test_eq(const char* name, auto expected, auto actual) -> void
    {
        if (expected == actual)
        {
            std::cout << "[PASS] " << name << std::endl;
            ++tests_passed;
        }
        else
        {
            std::cout << "[FAIL] " << name << " (expected: " << expected << ", actual: " << actual << ")" << std::endl;
            ++tests_failed;
        }
    }

    // === HTTP/2 帧测试 ===

    auto test_http2_frame_header() -> void
    {
        std::cout << "\n=== HTTP/2 Frame Header Tests ===\n";

        // 测试帧头部解析
        std::array<uint8_t, 9> header = {0x00, 0x00, 0x10, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};

        auto frame_opt = http2_frame::parse_header(header);
        test("Frame header parse success", frame_opt.has_value());

        if (frame_opt)
        {
            auto& frame = *frame_opt;
            test_eq("Frame length", 16u, frame.length);
            test_eq("Frame type", static_cast<uint8_t>(frame_type::settings), static_cast<uint8_t>(frame.type));
            test_eq("Frame flags", 0u, static_cast<uint32_t>(frame.flags));
            test_eq("Frame stream_id", 0u, frame.stream_id);
        }

        // 测试帧头部编码
        http2_frame encode_frame;
        encode_frame.length = 100;
        encode_frame.type = frame_type::data;
        encode_frame.flags = static_cast<uint8_t>(frame_flag::end_stream);
        encode_frame.stream_id = 1;

        auto encoded = encode_frame.encode_header();
        test_eq("Encoded length byte 0", 0u, static_cast<uint32_t>(encoded[0]));
        test_eq("Encoded length byte 1", 0u, static_cast<uint32_t>(encoded[1]));
        test_eq("Encoded length byte 2", 100u, static_cast<uint32_t>(encoded[2]));
        test_eq("Encoded type", static_cast<uint8_t>(frame_type::data), encoded[3]);
        test_eq("Encoded flags", static_cast<uint8_t>(frame_flag::end_stream), encoded[4]);
    }

    auto test_http2_settings() -> void
    {
        std::cout << "\n=== HTTP/2 Settings Tests ===\n";

        http2_settings settings;
        settings.header_table_size = 8192;
        settings.max_concurrent_streams = 50;
        settings.initial_window_size = 65535;

        auto encoded = settings.encode();
        test("Settings encode not empty", encoded.size() == 36); // 6 settings * 6 bytes

        auto decoded = http2_settings::decode(encoded);
        test_eq("Decoded header_table_size", 8192u, decoded.header_table_size);
        test_eq("Decoded max_concurrent_streams", 50u, decoded.max_concurrent_streams);
        test_eq("Decoded initial_window_size", 65535u, decoded.initial_window_size);
    }

    // === HPACK 测试 ===

    auto test_hpack_static_table() -> void
    {
        std::cout << "\n=== HPACK Static Table Tests ===\n";

        test("Static table size", hpack_static_table.size() >= 59);

        // 测试查找
        test_eq(":method GET index", 2, static_cast<int>(hpack_static_table[1].name.find(":method") != std::string_view::npos));
        test_eq(":status 200 index", 7, static_cast<int>(hpack_static_table[7].name.find(":status") != std::string_view::npos));
    }

    auto test_hpack_dynamic_table() -> void
    {
        std::cout << "\n=== HPACK Dynamic Table Tests ===\n";

        hpack_dynamic_table table(4096);

        // 添加条目
        table.add("custom-header", "custom-value");
        test("Dynamic table add", table.current_size() > 0);

        // 查找条目
        auto found = table.find("custom-header", "custom-value");
        test("Dynamic table find", found.has_value());

        // 大小限制
        hpack_dynamic_table small_table(100);
        small_table.add("very-long-header-name-that-exceeds-limit", "very-long-value");
        test("Dynamic table size limit", small_table.current_size() <= 100);
    }

    auto test_hpack_encoder_decoder() -> void
    {
        std::cout << "\n=== HPACK Encoder/Decoder Tests ===\n";

        hpack_encoder encoder;
        hpack_decoder decoder;

        std::vector<std::pair<std::string, std::string>> headers = {
            {":method", "GET"},
            {":path", "/"},
            {":scheme", "https"},
            {"host", "example.com"},
            {"custom-header", "custom-value"}
        };

        auto encoded = encoder.encode(headers);
        test("HPACK encode not empty", encoded.size() > 0);

        auto decoded_opt = decoder.decode(encoded);
        test("HPACK decode success", decoded_opt.has_value());

        if (decoded_opt)
        {
            auto& decoded = *decoded_opt;
            test_eq("Decoded header count", headers.size(), decoded.size());

            bool all_match = true;
            for (std::size_t i = 0; i < headers.size() && i < decoded.size(); ++i)
            {
                if (headers[i].first != decoded[i].first || headers[i].second != decoded[i].second)
                {
                    all_match = false;
                }
            }
            test("HPACK decoded headers match", all_match);
        }
    }

    // === Radix Tree 路由测试 ===

    auto test_radix_tree_static() -> void
    {
        std::cout << "\n=== Radix Tree Static Route Tests ===\n";

        radix_tree tree;

        // 插入静态路由
        tree.insert("/", [](context&) -> net::awaitable<void> { co_return; });
        tree.insert("/users", [](context&) -> net::awaitable<void> { co_return; });
        tree.insert("/users/list", [](context&) -> net::awaitable<void> { co_return; });
        tree.insert("/products", [](context&) -> net::awaitable<void> { co_return; });

        // 查找静态路由
        test("Root route found", tree.search("/").has_value());
        test("/users found", tree.search("/users").has_value());
        test("/users/list found", tree.search("/users/list").has_value());
        test("/products found", tree.search("/products").has_value());

        // 未匹配
        test("/notexist not found", !tree.search("/notexist").has_value());
        test("/users/detail not found", !tree.search("/users/detail").has_value());
    }

    auto test_radix_tree_params() -> void
    {
        std::cout << "\n=== Radix Tree Parameter Tests ===\n";

        radix_tree tree;

        // 插入参数路由
        tree.insert("/users/<int:id>", [](context&) -> net::awaitable<void> { co_return; });
        tree.insert("/users/<string:name>", [](context&) -> net::awaitable<void> { co_return; });
        tree.insert("/files/<path:filename>", [](context&) -> net::awaitable<void> { co_return; });

        // 测试 int 参数
        auto int_match = tree.search("/users/123");
        test("/users/123 found", int_match.has_value());
        if (int_match)
        {
            test_eq("int param id", std::string("123"), int_match->params["id"]);
        }

        // 测试 string 参数
        auto str_match = tree.search("/users/john");
        test("/users/john found", str_match.has_value());
        if (str_match)
        {
            test_eq("string param name", std::string("john"), str_match->params["name"]);
        }

        // 测试 path 参数
        auto path_match = tree.search("/files/docs/readme.txt");
        test("/files/docs/readme.txt found", path_match.has_value());
        if (path_match)
        {
            test_eq("path param filename", std::string("docs/readme.txt"), path_match->params["filename"]);
        }

        // int 参数验证（非数字不应匹配）
        auto invalid_int = tree.search("/users/abc");
        test("/users/abc not match int", !invalid_int.has_value());
    }

    auto test_router() -> void
    {
        std::cout << "\n=== Router Tests ===\n";

        router r;

        r.get("/", [](context&) -> net::awaitable<void> { co_return; });
        r.get("/api/users", [](context&) -> net::awaitable<void> { co_return; });
        r.post("/api/users", [](context&) -> net::awaitable<void> { co_return; });
        r.get("/api/users/<int:id>", [](context&) -> net::awaitable<void> { co_return; });

        // 方法匹配
        auto get_root = r.match(http::verb::get, "/");
        test("GET / found", get_root.has_value());

        auto post_root = r.match(http::verb::post, "/");
        test("POST / not found", !post_root.has_value());

        // GET /api/users
        auto get_users = r.match(http::verb::get, "/api/users");
        test("GET /api/users found", get_users.has_value());

        // POST /api/users
        auto post_users = r.match(http::verb::post, "/api/users");
        test("POST /api/users found", post_users.has_value());

        // 参数路由
        auto get_user_id = r.match(http::verb::get, "/api/users/42");
        test("GET /api/users/42 found", get_user_id.has_value());
        if (get_user_id)
        {
            test_eq("User id param", std::string("42"), get_user_id->params["id"]);
        }
    }

    // === 指标测试 ===

    auto test_metrics_counters() -> void
    {
        std::cout << "\n=== Metrics Counter Tests ===\n";

        server_metrics metrics;

        metrics.inc_requests();
        metrics.inc_requests();
        metrics.inc_requests();
        test_eq("Requests count", 3u, metrics.requests_total());

        metrics.inc_errors();
        test_eq("Errors count", 1u, metrics.errors_total());

        metrics.inc_success();
        metrics.inc_success();
        test_eq("Success count", 2u, metrics.success_total());

        metrics.inc_timeout();
        test_eq("Timeouts count", 1u, metrics.timeouts_total());
    }

    auto test_metrics_gauges() -> void
    {
        std::cout << "\n=== Metrics Gauge Tests ===\n";

        server_metrics metrics;

        metrics.set_active_connections(10);
        test_eq("Active connections set", 10u, metrics.active_connections());

        metrics.inc_active_connections();
        test_eq("Active connections inc", 11u, metrics.active_connections());

        metrics.dec_active_connections();
        metrics.dec_active_connections();
        test_eq("Active connections dec", 9u, metrics.active_connections());
    }

    auto test_metrics_latency() -> void
    {
        std::cout << "\n=== Metrics Latency Tests ===\n";

        server_metrics metrics;

        metrics.record_latency(std::chrono::milliseconds(5));
        metrics.record_latency(std::chrono::milliseconds(10));
        metrics.record_latency(std::chrono::milliseconds(50));
        metrics.record_latency(std::chrono::milliseconds(100));
        metrics.record_latency(std::chrono::milliseconds(500));

        test_eq("Requests total after latency", 5u, metrics.requests_total());

        // P50 应该在某个范围内
        auto p50 = metrics.latency_p50();
        test("P50 latency exists", p50 > 0);
    }

    auto test_metrics_prometheus() -> void
    {
        std::cout << "\n=== Metrics Prometheus Export Tests ===\n";

        server_metrics metrics;
        metrics.inc_requests();
        metrics.inc_errors();
        metrics.set_active_connections(5);

        auto prom = metrics.to_prometheus();
        test("Prometheus output not empty", prom.size() > 0);
        test("Prometheus has requests_total", prom.find("wan_web_requests_total") != std::string::npos);
        test("Prometheus has errors_total", prom.find("wan_web_errors_total") != std::string::npos);
        test("Prometheus has active_connections", prom.find("wan_web_active_connections") != std::string::npos);
    }

    // === Cookie 测试 ===

    auto test_cookie_parse() -> void
    {
        std::cout << "\n=== Cookie Parse Tests ===\n";

        // 构造带 Cookie 的请求
        http::request<http::string_body> req(http::verb::get, "/", 11);
        req.set(http::field::cookie, "session=abc123; user=john");

        context ctx(std::move(req));

        test_eq("Cookie session", std::string_view("abc123"), ctx.cookie("session"));
        test_eq("Cookie user", std::string_view("john"), ctx.cookie("user"));
        test("Cookie has session", ctx.has_cookie("session"));
        test("Cookie has user", ctx.has_cookie("user"));
        test("Cookie not found empty", ctx.cookie("notexist").empty());
    }

    auto test_cookie_set() -> void
    {
        std::cout << "\n=== Cookie Set Tests ===\n";

        http::request<http::string_body> req(http::verb::get, "/", 11);
        context ctx(std::move(req));

        ctx.set_cookie("test", "value", {
            .max_age = std::chrono::seconds(3600),
            .path = "/",
            .secure = true,
            .http_only = true,
            .same_site = "strict"
        });

        auto& resp = ctx.raw_response();
        auto cookie_header = resp[http::field::set_cookie];

        test("Set-Cookie not empty", !cookie_header.empty());
        test("Set-Cookie has name=value", cookie_header.find("test=value") != std::string_view::npos);
        test("Set-Cookie has Max-Age", cookie_header.find("Max-Age=3600") != std::string_view::npos);
        test("Set-Cookie has Secure", cookie_header.find("Secure") != std::string_view::npos);
        test("Set-Cookie has HttpOnly", cookie_header.find("HttpOnly") != std::string_view::npos);
        test("Set-Cookie has SameSite", cookie_header.find("SameSite=strict") != std::string_view::npos);
    }

    // === Session 测试 ===

    auto test_session_basic() -> void
    {
        std::cout << "\n=== Session Basic Tests ===\n";

        session sess("test_session_id");

        test_eq("Session id", std::string_view("test_session_id"), sess.id());
        test("Session not destroyed initially", !sess.destroyed());
        test("Session not modified initially", !sess.modified());

        sess.set("key1", "value1");
        sess.set("key2", "value2");
        test("Session modified after set", sess.modified());

        auto val1 = sess.get("key1");
        test("Session get key1 success", val1.has_value());
        test_eq("Session get key1 value", std::string("value1"), *val1);

        test("Session exists key1", sess.exists("key1"));
        test("Session not exists key3", !sess.exists("key3"));

        sess.erase("key1");
        test("Session not exists after erase", !sess.exists("key1"));

        sess.destroy();
        test("Session destroyed", sess.destroyed());
    }

    // === 连接限制测试 ===

    auto test_connection_limiter() -> void
    {
        std::cout << "\n=== Connection Limiter Tests ===\n";

        net::io_context ioc;
        auto strand = net::make_strand(ioc);

        connection_limiter limiter(strand, 5);

        test_eq("Limiter max_connections", 5u, limiter.max_connections());
        test_eq("Limiter initial active", 0u, limiter.active());

        // 运行 io_context 处理 strand 操作
        ioc.run_for(std::chrono::milliseconds(100));

        test("Limiter after run", limiter.active() == 0);
    }

    // === 服务器配置测试 ===

    auto test_server_config() -> void
    {
        std::cout << "\n=== Server Config Tests ===\n";

        server_config config;
        config.threads = 4;
        config.max_connections = 1000;
        config.max_body_size = 20 * 1024 * 1024;
        config.keep_alive_enabled = true;
        config.max_keepalive_requests = 200;

        test_eq("Config threads", 4u, config.threads);
        test_eq("Config max_connections", 1000u, config.max_connections);
        test_eq("Config max_body_size", 20 * 1024 * 1024u, config.max_body_size);
        test("Config keep_alive_enabled", config.keep_alive_enabled);
        test_eq("Config max_keepalive_requests", 200u, config.max_keepalive_requests);
    }

    // === 主测试入口 ===

    auto run_all_tests() -> void
    {
        std::cout << "========================================\n";
        std::cout << "wan/web Module Tests\n";
        std::cout << "========================================\n";

        // HTTP/2
        test_http2_frame_header();
        test_http2_settings();

        // HPACK
        test_hpack_static_table();
        test_hpack_dynamic_table();
        test_hpack_encoder_decoder();

        // Radix Tree 路由
        test_radix_tree_static();
        test_radix_tree_params();
        test_router();

        // 指标
        test_metrics_counters();
        test_metrics_gauges();
        test_metrics_latency();
        test_metrics_prometheus();

        // Cookie
        test_cookie_parse();
        test_cookie_set();

        // Session
        test_session_basic();

        // 连接限制
        test_connection_limiter();

        // 服务器配置
        test_server_config();

        std::cout << "\n========================================\n";
        std::cout << "Results: " << tests_passed << " passed, " << tests_failed << " failed\n";
        std::cout << "========================================\n";
    }
}

int main()
{
    wan::web::run_all_tests();
    return wan::web::tests_failed > 0 ? 1 : 0;
}