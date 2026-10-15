// SPDX-License-Identifier: MIT
#define BOOST_TEST_MODULE TlsTests

#include <boost/test/included/unit_test.hpp>

#include "../Include/Wan.hpp"

#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <utility>

namespace
{
    namespace Net = boost::asio;
    namespace Http = boost::beast::http;

    constexpr std::string_view PrivateKeyPassword =
        "wan-tls-test-password";

    struct TestTlsFixtures
    {
        std::filesystem::path RootCertificateFile;
        std::filesystem::path RootPrivateKeyFile;
        std::filesystem::path ServerCertificateFile;
        std::filesystem::path ServerPrivateKeyFile;
        std::filesystem::path EncryptedServerPrivateKeyFile;
        std::string RootCertificate;
        std::string ServerCertificate;
        std::string ServerPrivateKey;
        std::string EncryptedServerPrivateKey;
    };

    auto ReadFile(const std::filesystem::path& Path) -> std::string
    {
        std::ifstream Input{Path, std::ios::binary};
        if (!Input)
        {
            throw std::runtime_error("unable to read test TLS fixture");
        }
        return {std::istreambuf_iterator<char>{Input},
            std::istreambuf_iterator<char>{}};
    }

    auto Fixtures() -> const TestTlsFixtures&
    {
        static const auto Value = []
        {
            const auto TestDirectory =
                std::filesystem::path{__FILE__}.parent_path();
            const auto Directory = TestDirectory / "Data" / "Tls";
            TestTlsFixtures Result;
            Result.RootCertificateFile = Directory / "RootCertificate.pem";
            Result.RootPrivateKeyFile = Directory / "RootPrivateKey.pem";
            Result.ServerCertificateFile = Directory / "ServerCertificate.pem";
            Result.ServerPrivateKeyFile = Directory / "ServerPrivateKey.pem";
            Result.EncryptedServerPrivateKeyFile =
                Directory / "EncryptedServerPrivateKey.pem";
            Result.RootCertificate = ReadFile(Result.RootCertificateFile);
            Result.ServerCertificate = ReadFile(Result.ServerCertificateFile);
            Result.ServerPrivateKey = ReadFile(Result.ServerPrivateKeyFile);
            Result.EncryptedServerPrivateKey = ReadFile(
                Result.EncryptedServerPrivateKeyFile);
            return Result;
        }();
        return Value;
    }

    auto AcquireLoopbackPort() -> std::uint16_t
    {
        Net::io_context Ioc;
        Net::ip::tcp::acceptor Acceptor{Ioc,
            Net::ip::tcp::endpoint{Net::ip::tcp::v4(), 0}};
        return Acceptor.local_endpoint().port();
    }

    auto MakeMemoryCredentials() -> Wan::Web::TlsServerConfig
    {
        const auto& TestData = Fixtures();
        Wan::Web::TlsServerConfig Config;
        Config.CertificateChainPem = TestData.ServerCertificate;
        Config.PrivateKeyPem = TestData.ServerPrivateKey;
        return Config;
    }

    auto MakeClientConfig(bool UsePem) -> Wan::Web::HttpClientConfig
    {
        const auto& TestData = Fixtures();
        Wan::Web::HttpClientConfig Config;
        if (UsePem)
        {
            Config.Tls.CaCertificatePem.push_back(
                TestData.RootCertificate);
        }
        else
        {
            Config.Tls.CaCertificateFiles.push_back(
                TestData.RootCertificateFile);
        }
        return Config;
    }

    auto PerformGet(std::string Url,
        Wan::Web::HttpClientConfig Config = {})
        -> Wan::Web::ClientResponse
    {
        Net::io_context Ioc;
        Wan::Web::HttpClient Client{Ioc.get_executor(), std::move(Config)};
        std::optional<Wan::Web::ClientResponse> Response;
        std::exception_ptr Failure;
        Net::co_spawn(Ioc, Client.Get(Url),
            [&](std::exception_ptr Error,
                Wan::Web::ClientResponse Result)
            {
                Failure = Error;
                if (!Error)
                {
                    Response.emplace(std::move(Result));
                }
            });
        Ioc.run();
        if (Failure)
        {
            std::rethrow_exception(Failure);
        }
        if (!Response)
        {
            throw std::runtime_error(
                "HTTP client completed without a response");
        }
        return std::move(*Response);
    }

    auto ActiveConnections(const Wan::Web::Application& Application)
        -> std::uint64_t
    {
        const auto Metrics = Application.GetMetrics()->ToPrometheus();
        constexpr std::string_view Name = "\nwan_active_connections ";
        const auto Start = Metrics.find(Name);
        if (Start == std::string::npos)
        {
            throw std::runtime_error("active connection metric is missing");
        }
        const auto ValueStart = Start + Name.size();
        const auto End = Metrics.find('\n', ValueStart);
        return std::stoull(Metrics.substr(ValueStart, End - ValueStart));
    }

    auto WaitForActiveConnection(const Wan::Web::Application& Application)
        -> bool
    {
        for (int Attempt = 0; Attempt < 100; ++Attempt)
        {
            if (ActiveConnections(Application) > 0)
            {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds{5});
        }
        return false;
    }

    void CheckTlsFault(const std::function<void()>& Operation,
        Wan::Fault::Code Expected)
    {
        bool Matched = false;
        try
        {
            Operation();
        }
        catch (const boost::system::system_error& Error)
        {
            Matched = Error.code() == Expected;
        }
        BOOST_CHECK(Matched);
    }
}

BOOST_AUTO_TEST_CASE(TlsServerLoadsFileAndMemoryCredentials)
{
    static_assert(std::is_base_of_v<Wan::Web::Server, Wan::Web::TlsServer>);
    const auto& TestData = Fixtures();

    Wan::Web::TlsServerConfig FileConfig;
    FileConfig.CertificateChainFile = TestData.ServerCertificateFile;
    FileConfig.PrivateKeyFile = TestData.ServerPrivateKeyFile;
    auto FileServer = std::make_shared<Wan::Web::TlsServer>(
        Wan::Web::ServerConfig{}, FileConfig);
    BOOST_TEST(FileServer->GetMetrics() != nullptr);

    auto MemoryConfig = MakeMemoryCredentials();
    int PasswordCalls = 0;
    MemoryConfig.PrivateKeyPem = TestData.EncryptedServerPrivateKey;
    MemoryConfig.PrivateKeyPasswordCallback = [&]
    {
        ++PasswordCalls;
        return std::string{PrivateKeyPassword};
    };
    auto MemoryServer = std::make_shared<Wan::Web::TlsServer>(
        Wan::Web::ServerConfig{}, MemoryConfig);
    BOOST_TEST(MemoryServer->GetMetrics() != nullptr);
    BOOST_TEST(PasswordCalls > 0);

    Wan::Web::TlsServerConfig Invalid;
    BOOST_CHECK_THROW((void)std::make_shared<Wan::Web::TlsServer>(
        Wan::Web::ServerConfig{}, Invalid), std::invalid_argument);

    Invalid = MakeMemoryCredentials();
    Invalid.CertificateChainFile = TestData.ServerCertificateFile;
    BOOST_CHECK_THROW((void)std::make_shared<Wan::Web::TlsServer>(
        Wan::Web::ServerConfig{}, Invalid), std::invalid_argument);

    Invalid = MakeMemoryCredentials();
    Invalid.PrivateKeyPem = ReadFile(TestData.RootPrivateKeyFile);
    CheckTlsFault([&]
        {
            (void)std::make_shared<Wan::Web::TlsServer>(
                Wan::Web::ServerConfig{}, Invalid);
        }, Wan::Fault::Code::SslKeyLoadFailed);
}

BOOST_AUTO_TEST_CASE(ApplicationHostsVerifiedHttpsAndHttpClientVerifiesIdentity)
{
    Wan::Web::ApplicationConfig Config;
    Config.Server.Threads = 2;
    Config.Server.ReadTimeout = std::chrono::seconds{5};
    Config.Server.WriteTimeout = std::chrono::seconds{5};
    Config.Tls = MakeMemoryCredentials();

    Wan::Web::Application Application{Config};
    const auto Port = AcquireLoopbackPort();
    Application.GetServer().Listen(Port).Get("/secure",
        [](Wan::Web::Context& Context) -> Net::awaitable<void>
        {
            Context.Text("tls-ok");
            co_return;
        });
    Application.GetServer().Post("/echo",
        [](Wan::Web::Context& Context) -> Net::awaitable<void>
        {
            Context.Text(Context.Body());
            co_return;
        });

    Application.Start();
    const auto Url = "https://localhost:" + std::to_string(Port)
        + "/secure#ignored-fragment";
    const auto Response = PerformGet(Url, MakeClientConfig(false));
    BOOST_TEST(Response.Status == Http::status::ok);
    BOOST_TEST(Response.Body == "tls-ok");

    Net::io_context Ioc;
    Wan::Web::HttpClient Client{Ioc.get_executor(), MakeClientConfig(true)};
    std::optional<Wan::Web::ClientResponse> PostResponse;
    std::exception_ptr PostFailure;
    Net::co_spawn(Ioc,
        Client.Post("https://localhost:" + std::to_string(Port) + "/echo",
            "payload"),
        [&](std::exception_ptr Error, Wan::Web::ClientResponse Result)
        {
            PostFailure = Error;
            if (!Error)
            {
                PostResponse.emplace(std::move(Result));
            }
        });
    Ioc.run();
    if (PostFailure)
    {
        std::rethrow_exception(PostFailure);
    }
    BOOST_REQUIRE(PostResponse.has_value());
    BOOST_TEST(PostResponse->Status == Http::status::ok);
    BOOST_TEST(PostResponse->Body == "payload");

    Application.Stop();
    Application.Wait();
    BOOST_TEST(ActiveConnections(Application) == 0);
}

BOOST_AUTO_TEST_CASE(HttpClientRejectsUntrustedAndWrongHostCertificates)
{
    Wan::Web::ApplicationConfig Config;
    Config.Server.Threads = 1;
    Config.Tls = MakeMemoryCredentials();
    Wan::Web::Application Application{Config};
    const auto Port = AcquireLoopbackPort();
    Application.GetServer().Listen(Port).Get("/secure",
        [](Wan::Web::Context& Context) -> Net::awaitable<void>
        {
            Context.Text("unreachable-on-invalid-identity");
            co_return;
        });
    Application.Start();

    const auto Url = "https://localhost:" + std::to_string(Port) + "/secure";
    CheckTlsFault([&]
        {
            (void)PerformGet(Url);
        }, Wan::Fault::Code::TlsHandshakeFailed);

    CheckTlsFault([&]
        {
            (void)PerformGet("https://127.0.0.1:" + std::to_string(Port)
                    + "/secure",
                MakeClientConfig(true));
        }, Wan::Fault::Code::TlsHandshakeFailed);

    Application.Stop();
    Application.Wait();
    BOOST_TEST(ActiveConnections(Application) == 0);
}

BOOST_AUTO_TEST_CASE(HttpClientSupportsCrossSchemeRedirectsAndBlocksDowngrades)
{
    Wan::Web::ServerConfig PlainConfig;
    PlainConfig.Threads = 1;
    auto PlainServer = std::make_shared<Wan::Web::Server>(PlainConfig);
    const auto PlainPort = AcquireLoopbackPort();
    const auto TlsPort = AcquireLoopbackPort();
    std::atomic_bool PlainTargetCalled{false};
    PlainServer->Listen(PlainPort).Get("/to-https",
        [TlsPort](Wan::Web::Context& Context) -> Net::awaitable<void>
        {
            Context.Status(Http::status::found);
            Context.Response().set(Http::field::location,
                "https://localhost:" + std::to_string(TlsPort) + "/secure");
            co_return;
        });
    PlainServer->Get("/target",
        [&PlainTargetCalled](Wan::Web::Context& Context)
            -> Net::awaitable<void>
        {
            PlainTargetCalled.store(true, std::memory_order_release);
            Context.Text("plain-target");
            co_return;
        });

    Wan::Web::ApplicationConfig TlsConfig;
    TlsConfig.Server.Threads = 1;
    TlsConfig.Tls = MakeMemoryCredentials();
    Wan::Web::Application TlsApplication{TlsConfig};
    TlsApplication.GetServer().Listen(TlsPort).Get("/secure",
        [](Wan::Web::Context& Context) -> Net::awaitable<void>
        {
            Context.Text("secure-target");
            co_return;
        });
    TlsApplication.GetServer().Get("/relative",
        [](Wan::Web::Context& Context) -> Net::awaitable<void>
        {
            Context.Status(Http::status::found);
            Context.Response().set(Http::field::location, "secure");
            co_return;
        });
    TlsApplication.GetServer().Get("/scheme-relative",
        [TlsPort](Wan::Web::Context& Context) -> Net::awaitable<void>
        {
            Context.Status(Http::status::found);
            Context.Response().set(Http::field::location,
                "//localhost:" + std::to_string(TlsPort) + "/secure");
            co_return;
        });
    TlsApplication.GetServer().Get("/to-http",
        [PlainPort](Wan::Web::Context& Context) -> Net::awaitable<void>
        {
            Context.Status(Http::status::found);
            Context.Response().set(Http::field::location,
                "http://localhost:" + std::to_string(PlainPort) + "/target");
            co_return;
        });

    PlainServer->Start();
    TlsApplication.Start();

    const auto Upgraded = PerformGet(
        "http://localhost:" + std::to_string(PlainPort) + "/to-https",
        MakeClientConfig(true));
    BOOST_TEST(Upgraded.Status == Http::status::ok);
    BOOST_TEST(Upgraded.Body == "secure-target");

    const auto Relative = PerformGet(
        "https://localhost:" + std::to_string(TlsPort) + "/relative",
        MakeClientConfig(true));
    BOOST_TEST(Relative.Status == Http::status::ok);
    BOOST_TEST(Relative.Body == "secure-target");

    const auto SchemeRelative = PerformGet(
        "https://localhost:" + std::to_string(TlsPort) + "/scheme-relative",
        MakeClientConfig(true));
    BOOST_TEST(SchemeRelative.Status == Http::status::ok);
    BOOST_TEST(SchemeRelative.Body == "secure-target");

    auto RedirectConfig = MakeClientConfig(true);
    const auto Blocked = PerformGet(
        "https://localhost:" + std::to_string(TlsPort) + "/to-http",
        RedirectConfig);
    BOOST_TEST(Blocked.Status == Http::status::found);
    BOOST_TEST(!PlainTargetCalled.load(std::memory_order_acquire));

    RedirectConfig.AllowInsecureRedirects = true;
    const auto Allowed = PerformGet(
        "https://localhost:" + std::to_string(TlsPort) + "/to-http",
        std::move(RedirectConfig));
    BOOST_TEST(Allowed.Status == Http::status::ok);
    BOOST_TEST(Allowed.Body == "plain-target");

    TlsApplication.Stop();
    TlsApplication.Wait();
    PlainServer->Stop();
    PlainServer->Wait();
    BOOST_TEST(ActiveConnections(TlsApplication) == 0);
}

BOOST_AUTO_TEST_CASE(TlsApplicationCancelsAClientHelloDuringGracefulStop)
{
    Wan::Web::ApplicationConfig Config;
    Config.Server.Threads = 1;
    Config.Server.ReadTimeout = std::chrono::seconds::zero();
    Config.DrainTimeout = std::chrono::seconds::zero();
    Config.Tls = MakeMemoryCredentials();

    Wan::Web::Application Application{Config};
    const auto Port = AcquireLoopbackPort();
    Application.GetServer().Listen(Port);
    Application.Start();

    Net::io_context Ioc;
    Net::ip::tcp::socket Socket{Ioc};
    Socket.connect({Net::ip::address_v4::loopback(), Port});
    BOOST_REQUIRE(WaitForActiveConnection(Application));

    Application.Stop();
    Application.Wait();
    BOOST_TEST(static_cast<std::uint8_t>(Application.State())
        == static_cast<std::uint8_t>(Wan::Web::ApplicationState::Stopped));
    BOOST_TEST(ActiveConnections(Application) == 0);

    char Byte = 0;
    boost::system::error_code Ec;
    Socket.read_some(Net::buffer(&Byte, 1), Ec);
    BOOST_CHECK(Ec == Net::error::eof
        || Ec == Net::error::connection_reset
        || Ec == Net::error::operation_aborted);
}

BOOST_AUTO_TEST_CASE(HttpClientRejectsUnsupportedAbsoluteUrlSchemes)
{
    BOOST_CHECK_THROW((void)PerformGet("ftp://localhost/file"),
        std::invalid_argument);
    BOOST_CHECK_THROW((void)PerformGet("https://user@localhost/"),
        std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(HttpClientUsesTheSchemeDefaultPort)
{
    const auto HttpUrl = Wan::Web::Detail::ParseHttpUrl(
        "http://localhost/health");
    BOOST_REQUIRE(HttpUrl.has_value());
    BOOST_TEST(HttpUrl->Port == "80");

    const auto HttpsUrl = Wan::Web::Detail::ParseHttpUrl(
        "https://localhost/health");
    BOOST_REQUIRE(HttpsUrl.has_value());
    BOOST_TEST(HttpsUrl->Port == "443");
}

BOOST_AUTO_TEST_CASE(TlsApplicationGracefullyDrainsAnActiveRequest)
{
    Wan::Web::ApplicationConfig Config;
    Config.Server.Threads = 1;
    Config.Server.ReadTimeout = std::chrono::seconds{5};
    Config.Server.WriteTimeout = std::chrono::seconds{5};
    Config.DrainTimeout = std::chrono::seconds{5};
    Config.Tls = MakeMemoryCredentials();

    Wan::Web::Application Application{Config};
    const auto Port = AcquireLoopbackPort();
    std::atomic_bool HandlerStarted{false};
    Application.GetServer().Listen(Port).Get("/slow",
        [&HandlerStarted](Wan::Web::Context& Context)
            -> Net::awaitable<void>
        {
            HandlerStarted.store(true, std::memory_order_release);
            Net::steady_timer Timer{
                co_await Net::this_coro::executor,
                std::chrono::milliseconds{100}};
            co_await Timer.async_wait(Net::use_awaitable);
            Context.Text("drained");
        });
    Application.Start();

    std::optional<Wan::Web::ClientResponse> Response;
    std::exception_ptr Failure;
    const auto Url = "https://localhost:" + std::to_string(Port) + "/slow";
    std::thread ClientThread{[&]
    {
        try
        {
            Response = PerformGet(Url, MakeClientConfig(true));
        }
        catch (...)
        {
            Failure = std::current_exception();
        }
    }};

    bool Started = false;
    for (int Attempt = 0; Attempt < 200; ++Attempt)
    {
        if (HandlerStarted.load(std::memory_order_acquire))
        {
            Started = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }

    if (Started)
    {
        Application.Stop();
    }
    else
    {
        Application.GetServer().ForceStop();
    }
    ClientThread.join();
    Application.Wait();

    BOOST_REQUIRE(Started);
    if (Failure)
    {
        std::rethrow_exception(Failure);
    }
    BOOST_REQUIRE(Response.has_value());
    BOOST_TEST(Response->Status == Http::status::ok);
    BOOST_TEST(Response->Body == "drained");
    BOOST_TEST(ActiveConnections(Application) == 0);
    BOOST_TEST(static_cast<std::uint8_t>(Application.State())
        == static_cast<std::uint8_t>(Wan::Web::ApplicationState::Stopped));
}
