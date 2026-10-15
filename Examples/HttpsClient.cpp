/**
 * @file HttpsClient.cpp
 * @brief 使用系统信任根或自定义 CA 访问 HTTPS URL。
 */
#include <Wan.hpp>

#include <exception>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

int main(int Argc, char** Argv)
{
    if (Argc < 2 || Argc > 3)
    {
        std::cerr << "Usage: HttpsClient <http(s)-url> [ca-certificate.pem]\n";
        return 2;
    }

    try
    {
        Wan::Web::HttpClientConfig Config;
        if (Argc == 3)
        {
            Config.Tls.CaCertificateFiles.emplace_back(Argv[2]);
        }

        boost::asio::io_context Ioc;
        Wan::Web::HttpClient Client{Ioc.get_executor(), std::move(Config)};
        std::optional<Wan::Web::ClientResponse> Response;
        std::exception_ptr Failure;
        boost::asio::co_spawn(Ioc, Client.Get(Argv[1]),
            [&](std::exception_ptr Error, Wan::Web::ClientResponse Result)
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
            throw std::runtime_error("HTTPS request completed without a response");
        }

        std::cout << "HTTP status: "
            << static_cast<unsigned int>(Response->Status) << "\n"
            << Response->Body << '\n';
        return 0;
    }
    catch (const std::exception& Error)
    {
        std::cerr << "HTTPS request failed: " << Error.what() << '\n';
        return 1;
    }
}
