/**
 * @file HttpsServer.cpp
 * @brief 使用证书文件启动一个简单的 HTTPS 服务。
 */
#include <Wan.hpp>

#include <cstdint>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

int main(int Argc, char** Argv)
{
    if (Argc < 3 || Argc > 4)
    {
        std::cerr << "Usage: HttpsServer <certificate-chain.pem> "
            "<private-key.pem> [port]\n";
        return 2;
    }

    try
    {
        std::uint16_t Port = 8443;
        if (Argc == 4)
        {
            const auto ParsedPort = std::stoul(Argv[3]);
            if (ParsedPort == 0 || ParsedPort > 65535)
            {
                throw std::invalid_argument(
                    "port must be between 1 and 65535");
            }
            Port = static_cast<std::uint16_t>(ParsedPort);
        }

        Wan::Web::TlsServerConfig Tls;
        Tls.CertificateChainFile = Argv[1];
        Tls.PrivateKeyFile = Argv[2];

        Wan::Web::ApplicationConfig Config;
        Config.Tls = std::move(Tls);

        Wan::Web::Application Application{std::move(Config)};
        Application.GetServer().Listen(Port).Get("/health",
            [](Wan::Web::Context& Context)
                -> boost::asio::awaitable<void>
            {
                Context.Json(R"({"status":"ok"})");
                co_return;
            });

        Application.Start();
        std::cout << "HTTPS listening on port " << Port
            << ". Press Enter to stop.\n";
        std::cin.get();
        Application.Stop();
        Application.Wait();
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "HTTPS server failed: " << Error.what() << '\n';
        return 1;
    }
}
