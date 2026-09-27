// SPDX-License-Identifier: MIT
#define BOOST_TEST_MODULE ApplicationTests

#include <boost/test/included/unit_test.hpp>

#include "../Include/Wan.hpp"
#include "../Wan/Web/Server/ConnectionRegistry.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

namespace
{
    auto AcquireLoopbackPort() -> std::uint16_t
    {
        boost::asio::io_context Ioc;
        boost::asio::ip::tcp::acceptor Acceptor{Ioc,
            boost::asio::ip::tcp::endpoint{
                boost::asio::ip::tcp::v4(), 0}};
        const auto Port = Acceptor.local_endpoint().port();
        boost::system::error_code Ec;
        Ec = Acceptor.close(Ec);
        return Port;
    }
}

BOOST_AUTO_TEST_CASE(ApplicationPublicContractCompiles)
{
    static_assert(!std::is_copy_constructible_v<Wan::Web::Application>);
    static_assert(!std::is_move_constructible_v<Wan::Web::Application>);

    Wan::Web::Application Application;
    BOOST_TEST(static_cast<std::uint8_t>(Application.State())
        == static_cast<std::uint8_t>(Wan::Web::ApplicationState::Created));
    BOOST_TEST(Application.GetMetrics() != nullptr);

    Application.GetServer().Get("/ready",
        [](Wan::Web::Context&) -> boost::asio::awaitable<void>
        {
            co_return;
        });
}

BOOST_AUTO_TEST_CASE(ApplicationCanStopBeforeStart)
{
    Wan::Web::Application Application;
    Application.Stop();
    Application.Wait();
    BOOST_TEST(static_cast<std::uint8_t>(Application.State())
        == static_cast<std::uint8_t>(Wan::Web::ApplicationState::Stopped));
}

BOOST_AUTO_TEST_CASE(ConnectionRegistryTracksAndClosesStreams)
{
    boost::asio::io_context Ioc;
    Wan::Web::ConnectionRegistry Registry;
    auto First = std::make_shared<boost::beast::tcp_stream>(
        Ioc.get_executor());
    auto Second = std::make_shared<boost::beast::tcp_stream>(
        Ioc.get_executor());

    BOOST_CHECK_THROW((void)Registry.Register(nullptr), std::invalid_argument);
    const auto FirstId = Registry.Register(First);
    const auto SecondId = Registry.Register(Second);
    BOOST_TEST(FirstId != SecondId);
    BOOST_TEST(Registry.Size() == 2);

    Registry.Remove(FirstId);
    Registry.Remove(FirstId);
    BOOST_TEST(Registry.Size() == 1);

    Registry.CloseAll();
    (void)Ioc.poll();
    BOOST_TEST(Registry.Size() == 0);
    BOOST_TEST(!Second->socket().is_open());
    Registry.Remove(SecondId);

    boost::asio::io_context StoppedIoc;
    auto StoppedStream = std::make_shared<boost::beast::tcp_stream>(
        StoppedIoc.get_executor());
    StoppedStream->socket().open(boost::asio::ip::tcp::v4());
    Wan::Web::ConnectionRegistry StoppedRegistry;
    (void)StoppedRegistry.Register(StoppedStream);
    StoppedRegistry.CloseAllNow();
    BOOST_TEST(!StoppedStream->socket().is_open());
}

BOOST_AUTO_TEST_CASE(ServerStartAndForceStopReleasesWorkers)
{
    Wan::Web::ServerConfig Config;
    Config.Threads = 1;
    Config.ReuseAddress = false;
    auto Server = std::make_shared<Wan::Web::Server>(Config);
    Server->Listen(0);

    Server->Start();
    Server->ForceStop();
    Server->Wait();
}

BOOST_AUTO_TEST_CASE(ServerForceStopBeforeStartIsHarmless)
{
    auto Server = std::make_shared<Wan::Web::Server>();
    Server->ForceStop();
    Server->Listen(0);
    Server->Start();
    Server->ForceStop();
    Server->Wait();
}

BOOST_AUTO_TEST_CASE(ServerFreezesConfigurationAfterStart)
{
    auto Server = std::make_shared<Wan::Web::Server>();
    Server->Listen(0).Get("/before",
        [](Wan::Web::Context&) -> boost::asio::awaitable<void>
        {
            co_return;
        });

    Server->Start();
    BOOST_CHECK_THROW(Server->Get("/after",
        [](Wan::Web::Context&) -> boost::asio::awaitable<void>
        {
            co_return;
        }), std::logic_error);
    BOOST_CHECK_THROW(Server->GetRouter().Get("/router-after",
        [](Wan::Web::Context&) -> boost::asio::awaitable<void>
        {
            co_return;
        }), std::logic_error);
    Server->Stop();
    Server->Wait();
}

BOOST_AUTO_TEST_CASE(ServerStartRejectsOccupiedPort)
{
    boost::asio::io_context Ioc;
    boost::asio::ip::tcp::acceptor Probe{Ioc,
        boost::asio::ip::tcp::endpoint{
            boost::asio::ip::tcp::v4(), 0}};
    const auto Port = Probe.local_endpoint().port();

    Wan::Web::ServerConfig Config;
    Config.Threads = 1;
    Config.ReuseAddress = false;
    auto Server = std::make_shared<Wan::Web::Server>(Config);
    Server->Listen(Port);

    bool Failed = false;
    try
    {
        Server->Start();
    }
    catch (const std::exception&)
    {
        Failed = true;
    }
    BOOST_TEST(Failed);
    Server->ForceStop();
    bool WaitFailed = false;
    try
    {
        Server->Wait();
    }
    catch (const std::exception&)
    {
        WaitFailed = true;
    }
    BOOST_TEST(WaitFailed);
}

BOOST_AUTO_TEST_CASE(ServerForceStopFinalizesAcceptedConnection)
{
    boost::asio::io_context ProbeIoc;
    boost::asio::ip::tcp::acceptor Probe{ProbeIoc,
        boost::asio::ip::tcp::endpoint{
            boost::asio::ip::tcp::v4(), 0}};
    const auto Port = Probe.local_endpoint().port();
    Probe.close();

    Wan::Web::ServerConfig Config;
    Config.Threads = 1;
    Config.ReadTimeout = std::chrono::seconds{0};
    auto Server = std::make_shared<Wan::Web::Server>(Config);
    Server->Listen(Port);
    Server->Start();

    boost::asio::io_context ClientIoc;
    boost::asio::ip::tcp::socket ClientSocket{ClientIoc};
    boost::system::error_code ConnectEc;
    ConnectEc = ClientSocket.connect(
        {boost::asio::ip::make_address_v4("127.0.0.1"), Port}, ConnectEc);
    BOOST_REQUIRE(!ConnectEc);

    std::this_thread::sleep_for(std::chrono::milliseconds{50});
    Server->BeginGracefulStop(std::chrono::seconds{10});
    Server->ForceStop();
    Server->Wait();
    ConnectEc = ClientSocket.close(ConnectEc);

    const auto Metrics = Server->GetMetrics()->ToPrometheus();
    BOOST_TEST(Metrics.find("wan_active_connections 0")
        != std::string::npos);
    std::weak_ptr<Wan::Web::Server> WeakServer = Server;
    Server.reset();
    BOOST_TEST(WeakServer.expired());
}

BOOST_AUTO_TEST_CASE(ApplicationStartsAndGracefullyStops)
{
    Wan::Web::ApplicationConfig Config;
    Config.Server.Threads = 1;
    Config.DrainTimeout = std::chrono::seconds{1};
    Wan::Web::Application Application{Config};
    Application.GetServer().Listen(0);

    Application.Start();
    BOOST_TEST(static_cast<std::uint8_t>(Application.State())
        == static_cast<std::uint8_t>(Wan::Web::ApplicationState::Running));
    Application.Start();
    Application.Stop();
    Application.Wait();
    BOOST_TEST(static_cast<std::uint8_t>(Application.State())
        == static_cast<std::uint8_t>(Wan::Web::ApplicationState::Stopped));
}

BOOST_AUTO_TEST_CASE(ApplicationConcurrentStopIsIdempotent)
{
    Wan::Web::ApplicationConfig Config;
    Config.Server.Threads = 2;
    Config.Server.ReuseAddress = false;
    Wan::Web::Application Application{Config};
    Application.GetServer().Listen(AcquireLoopbackPort());
    Application.Start();

    std::thread FirstStop([&Application]() { Application.Stop(); });
    std::thread SecondStop([&Application]() { Application.Stop(); });
    FirstStop.join();
    SecondStop.join();
    Application.Wait();

    BOOST_TEST(static_cast<std::uint8_t>(Application.State())
        == static_cast<std::uint8_t>(Wan::Web::ApplicationState::Stopped));
}

BOOST_AUTO_TEST_CASE(ApplicationGracefulDrainCompletesKeepAliveConnection)
{
    Wan::Web::ApplicationConfig Config;
    Config.Server.Threads = 1;
    Config.Server.ReuseAddress = false;
    Config.DrainTimeout = std::chrono::seconds{2};
    Wan::Web::Application Application{Config};
    const auto Port = AcquireLoopbackPort();
    Application.GetServer().Listen(Port).Get("/drain",
        [](Wan::Web::Context& Context)
            -> boost::asio::awaitable<void>
        {
            Context.Text("drained");
            co_return;
        });
    Application.Start();

    boost::asio::io_context ClientIoc;
    boost::asio::ip::tcp::socket Socket{ClientIoc};
    boost::system::error_code Ec;
    Ec = Socket.connect(
        {boost::asio::ip::make_address_v4("127.0.0.1"), Port}, Ec);
    BOOST_REQUIRE(!Ec);

    boost::beast::http::request<boost::beast::http::string_body> Request{
        boost::beast::http::verb::get, "/drain", 11};
    Request.set(boost::beast::http::field::host, "localhost");
    Request.set(boost::beast::http::field::connection, "keep-alive");
    Request.prepare_payload();
    boost::beast::http::write(Socket, Request, Ec);
    BOOST_REQUIRE(!Ec);

    boost::beast::flat_buffer Buffer;
    boost::beast::http::response<boost::beast::http::string_body> Response;
    boost::beast::http::read(Socket, Buffer, Response, Ec);
    BOOST_REQUIRE(!Ec);
    BOOST_TEST(Response.body() == "drained");

    Application.Stop();
    Ec = Socket.close(Ec);
    Application.Wait();
    BOOST_TEST(Application.GetMetrics()->ToPrometheus().find(
        "wan_active_connections 0") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(ApplicationForceStopsBlockedHandlerAtDeadline)
{
    Wan::Web::ApplicationConfig Config;
    Config.Server.Threads = 1;
    Config.Server.ReuseAddress = false;
    Config.DrainTimeout = std::chrono::seconds{0};
    Wan::Web::Application Application{Config};
    const auto Port = AcquireLoopbackPort();
    std::atomic_bool HandlerStarted = false;
    Application.GetServer().Listen(Port).Get("/blocked",
        [&HandlerStarted](Wan::Web::Context& Context)
            -> boost::asio::awaitable<void>
        {
            HandlerStarted.store(true, std::memory_order_release);
            auto Ex = co_await boost::asio::this_coro::executor;
            boost::asio::steady_timer Timer{Ex, std::chrono::seconds{5}};
            boost::system::error_code Ec;
            co_await Timer.async_wait(boost::asio::redirect_error(
                boost::asio::use_awaitable, Ec));
            if (!Ec)
            {
                Context.Text("unexpected");
            }
            co_return;
        });
    Application.Start();

    boost::asio::io_context ClientIoc;
    boost::asio::ip::tcp::socket Socket{ClientIoc};
    boost::system::error_code Ec;
    Ec = Socket.connect(
        {boost::asio::ip::make_address_v4("127.0.0.1"), Port}, Ec);
    BOOST_REQUIRE(!Ec);

    boost::beast::http::request<boost::beast::http::string_body> Request{
        boost::beast::http::verb::get, "/blocked", 11};
    Request.set(boost::beast::http::field::host, "localhost");
    Request.set(boost::beast::http::field::connection, "close");
    Request.prepare_payload();
    boost::beast::http::write(Socket, Request, Ec);
    BOOST_REQUIRE(!Ec);

    for (int Attempt = 0; Attempt < 100
        && !HandlerStarted.load(std::memory_order_acquire); ++Attempt)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
    BOOST_REQUIRE(HandlerStarted.load(std::memory_order_acquire));

    const auto Begin = std::chrono::steady_clock::now();
    Application.Stop();
    Ec = Socket.close(Ec);
    Application.Wait();
    const auto Elapsed = std::chrono::steady_clock::now() - Begin;

    BOOST_TEST(Elapsed < std::chrono::seconds{2});
    BOOST_TEST(Application.GetMetrics()->ToPrometheus().find(
        "wan_active_connections 0") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(ServerRejectsOversizedHeaders)
{
    Wan::Web::ServerConfig Config;
    Config.Threads = 1;
    Config.ReuseAddress = false;
    Config.MaxHeaderSize = 64;
    auto Server = std::make_shared<Wan::Web::Server>(Config);
    const auto Port = AcquireLoopbackPort();
    std::atomic_bool HandlerCalled = false;
    Server->Listen(Port).Get("/oversized",
        [&HandlerCalled](Wan::Web::Context&)
            -> boost::asio::awaitable<void>
        {
            HandlerCalled.store(true, std::memory_order_release);
            co_return;
        });
    Server->Start();

    boost::asio::io_context ClientIoc;
    boost::asio::ip::tcp::socket Socket{ClientIoc};
    boost::system::error_code Ec;
    Ec = Socket.connect(
        {boost::asio::ip::make_address_v4("127.0.0.1"), Port}, Ec);
    BOOST_REQUIRE(!Ec);

    const auto Payload = std::string{
        "GET /oversized HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "X-Large: "}
        + std::string(128, 'x') + "\r\n\r\n";
    const auto Written = boost::asio::write(
        Socket, boost::asio::buffer(Payload), Ec);
    BOOST_REQUIRE(!Ec);
    BOOST_TEST(Written == Payload.size());

    std::array<char, 1> Byte{};
    const auto Read = Socket.read_some(boost::asio::buffer(Byte), Ec);
    (void)Read;
    BOOST_TEST(static_cast<bool>(Ec));
    BOOST_TEST(!HandlerCalled.load(std::memory_order_acquire));

    Ec = Socket.close(Ec);
    Server->Stop();
    Server->Wait();
}

BOOST_AUTO_TEST_CASE(ApplicationRejectsInvalidConfiguration)
{
    Wan::Web::ApplicationConfig Config;
    Config.Server.MaxBodySize = 0;
    Wan::Web::Application Application{Config};

    BOOST_CHECK_THROW(Application.Start(), std::invalid_argument);
    BOOST_TEST(static_cast<std::uint8_t>(Application.State())
        == static_cast<std::uint8_t>(Wan::Web::ApplicationState::Failed));
    Application.Stop();
    BOOST_CHECK_THROW(Application.Wait(), std::exception);
}
