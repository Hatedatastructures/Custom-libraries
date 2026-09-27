/**
 * @file Client.hpp
 * @brief HTTP 客户端
 * @details 基于 Beast 的异步 HTTP 客户端，支持重定向。
 *          RedirectCount 为请求局部变量，无状态累积。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/beast.hpp>

#include <cstdio>
#include <chrono>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace Wan::Web
{
    namespace Net = boost::asio;
    namespace Beast = boost::beast;
    namespace Http = Beast::http;

    /**
     * @struct ClientResponse
     * @brief 客户端响应
     */
    struct ClientResponse
    {
        /** @brief HTTP 状态码。 */
        Http::status Status;
        /** @brief 响应正文。 */
        std::string Body;
        /** @brief 响应头字段。 */
        Http::fields Headers;
    };

    /**
     * @struct HttpClientConfig
     * @brief HTTP 客户端的超时和响应大小限制。
     */
    struct HttpClientConfig
    {
        /** @brief 解析、连接、写入和读取阶段各自的超时。 */
        std::chrono::seconds Timeout{30};
        /** @brief 响应头最大字节数。 */
        std::size_t MaxResponseHeaderSize = 64 * 1024;
        /** @brief 响应正文最大字节数。 */
        std::size_t MaxResponseBodySize = std::size_t{64} * 1024 * 1024;
    };

    /**
     * @class HttpClient
     * @brief HTTP 客户端
     */
    class HttpClient
    {
    public:
        /**
         * @brief 创建 HTTP 客户端。
         * @param Executor 执行网络操作的 Asio 执行器。
         * @param Config 超时和响应大小限制。
         * @throws std::invalid_argument 配置中的超时或响应限制无效时抛出。
         */
        explicit HttpClient(Net::any_io_executor Executor,
            HttpClientConfig Config = {})
            : MemberExecutor(std::move(Executor))
            , MemberTimeout(Config.Timeout)
            , MemberMaxResponseHeaderSize(Config.MaxResponseHeaderSize)
            , MemberMaxResponseBodySize(Config.MaxResponseBodySize)
        {
            ValidateConfiguration();
        }

        /**
         * @brief 发送 GET 请求
         * @param Host 主机名。
         * @param Port 端口。
         * @param Target 请求路径。
         * @param MaxRedirects 最大重定向次数
         * @return HTTP 响应。
         * @throws std::invalid_argument `MaxRedirects` 为负数时抛出。
         * @throws boost::system::system_error 网络操作失败或超时时抛出。
         * @note 301、302 和 303 响应可能将非 GET/HEAD 请求改为 GET。
         */
        [[nodiscard]] auto Get(std::string_view Host,
            std::string_view Port, std::string_view Target,
            int MaxRedirects = 5) -> Net::awaitable<ClientResponse>
        {
            co_return co_await DoRequest(
                RequestParameters{Http::verb::get, std::string(Host),
                    std::string(Port), std::string(Target), {}},
                MaxRedirects);
        }

        /**
         * @brief 发送 POST 请求
         * @param Host 主机名。
         * @param Port 端口。
         * @param Target 请求路径。
         * @param Body 请求正文。
         * @param MaxRedirects 最大重定向次数。
         * @return HTTP 响应。
         * @throws std::invalid_argument `MaxRedirects` 为负数时抛出。
         * @throws boost::system::system_error 网络操作失败或超时时抛出。
         * @note 301、302 和 303 响应会将请求改为 GET；307 和 308 保留原方法。
         */
        [[nodiscard]] auto Post(std::string_view Host,
            std::string_view Port, std::string_view Target,
            std::string_view Body, int MaxRedirects = 5)
            -> Net::awaitable<ClientResponse>
        {
            co_return co_await DoRequest(
                RequestParameters{Http::verb::post, std::string(Host),
                    std::string(Port), std::string(Target),
                    std::string(Body)},
                MaxRedirects);
        }

    private:
        /** @brief 一次 HTTP 请求的目标和正文参数。 */
        struct RequestParameters
        {
            /** @brief HTTP 方法。 */
            Http::verb Method;
            /** @brief 目标主机名。 */
            std::string Host;
            /** @brief 目标服务端口。 */
            std::string Port;
            /** @brief 请求目标路径和查询串。 */
            std::string Target;
            /** @brief 请求正文。 */
            std::string Body;
        };

        /** @brief 解析后的绝对 HTTP 重定向目标。 */
        struct ParsedLocation
        {
            /** @brief 目标主机名。 */
            std::string Host;
            /** @brief 目标服务端口。 */
            std::string Port;
            /** @brief 目标路径和查询串。 */
            std::string Target;
        };

        /**
         * @brief 解析绝对 HTTP 重定向地址。
         * @param Location `http://` 形式的绝对地址。
         * @return 可解析时返回主机、端口和目标；格式不支持时返回空值。
         * @note 未指定端口时使用 80；未指定路径时使用 `/`。
         */
        [[nodiscard]] static auto ParseAbsoluteHttpLocation(
            std::string_view Location) -> std::optional<ParsedLocation>
        {
            constexpr std::string_view Scheme = "http://";
            if (!Location.starts_with(Scheme))
            {
                return std::nullopt;
            }

            auto AuthorityAndPath = Location.substr(Scheme.size());
            const auto Slash = AuthorityAndPath.find('/');
            const auto Authority = Slash == std::string_view::npos
                ? AuthorityAndPath
                : AuthorityAndPath.substr(0, Slash);
            if (Authority.empty())
            {
                return std::nullopt;
            }

            ParsedLocation Result;
            Result.Host = std::string(Authority);
            Result.Port = "80";
            if (Authority.front() == '[')
            {
                const auto ClosingBracket = Authority.find(']');
                if (ClosingBracket == std::string_view::npos)
                {
                    return std::nullopt;
                }
                Result.Host = std::string(
                    Authority.substr(0, ClosingBracket + 1));
                if (ClosingBracket + 1 < Authority.size())
                {
                    if (Authority[ClosingBracket + 1] != ':')
                    {
                        return std::nullopt;
                    }
                    Result.Port = std::string(
                        Authority.substr(ClosingBracket + 2));
                }
            }
            else
            {
                const auto Colon = Authority.rfind(':');
                if (Colon != std::string_view::npos
                    && Authority.find(':') == Colon)
                {
                    Result.Host = std::string(Authority.substr(0, Colon));
                    Result.Port = std::string(Authority.substr(Colon + 1));
                }
            }

            if (Result.Host.empty() || Result.Port.empty())
            {
                return std::nullopt;
            }
            Result.Target = Slash == std::string_view::npos
                ? "/"
                : std::string(AuthorityAndPath.substr(Slash));
            return Result;
        }

        /**
         * @brief 发送请求并按上限跟随支持的 HTTP 重定向。
         * @param Parameters 请求方法、目标、主机、端口和正文。
         * @param MaxRedirects 本请求允许跟随的最大重定向次数。
         * @return 收到的最终响应；达到上限时返回当前重定向响应。
         * @throws std::invalid_argument 最大重定向次数为负数时抛出。
         */
        auto DoRequest(RequestParameters Parameters, int MaxRedirects)
            -> Net::awaitable<ClientResponse>
        {
            if (MaxRedirects < 0)
            {
                throw std::invalid_argument(
                    "maximum redirects must not be negative");
            }
            int RedirectCount = 0;

            for (;;)
            {
                auto Resp = co_await SendRequest(Parameters);

                // 处理重定向
                auto Status = Resp.Status;
                if ((Status == Http::status::moved_permanently
                        || Status == Http::status::found
                        || Status == Http::status::see_other
                        || Status == Http::status::temporary_redirect
                        || Status == Http::status::permanent_redirect)
                    && RedirectCount < MaxRedirects)
                {
                    auto Location =
                        Resp.Headers.find(Http::field::location);
                    if (Location != Resp.Headers.end())
                    {
                        const auto LocationValue = Location->value();
                        if (LocationValue.starts_with("//"))
                        {
                            auto Parsed = ParseAbsoluteHttpLocation(
                                std::string("http:")
                                    + std::string(LocationValue));
                            if (!Parsed)
                            {
                                co_return Resp;
                            }
                            Parameters.Host = std::move(Parsed->Host);
                            Parameters.Port = std::move(Parsed->Port);
                            Parameters.Target = std::move(Parsed->Target);
                        }
                        else if (LocationValue.starts_with('/'))
                        {
                            Parameters.Target = std::string(LocationValue);
                        }
                        else if (LocationValue.starts_with('?'))
                        {
                            const auto Query = Parameters.Target.find('?');
                            Parameters.Target = Parameters.Target.substr(
                                0, Query);
                            Parameters.Target += std::string(LocationValue);
                        }
                        else
                        {
                            auto Parsed = ParseAbsoluteHttpLocation(LocationValue);
                            if (!Parsed)
                            {
                                co_return Resp;
                            }
                            Parameters.Host = std::move(Parsed->Host);
                            Parameters.Port = std::move(Parsed->Port);
                            Parameters.Target = std::move(Parsed->Target);
                        }
                        if ((Status == Http::status::moved_permanently
                                || Status == Http::status::found
                                || Status == Http::status::see_other)
                            && Parameters.Method != Http::verb::get
                            && Parameters.Method != Http::verb::head)
                        {
                            Parameters.Method = Http::verb::get;
                            Parameters.Body.clear();
                        }
                        ++RedirectCount;
                        continue;
                    }
                }

                co_return Resp;
            }
        }

        /**
         * @brief 解析地址并发送一次 HTTP 请求。
         * @param Parameters 本次请求的参数。
         * @return 收到的状态码、正文和响应头。
         * @note DNS、连接、写入和读取各阶段使用配置超时；解析器限制响应头与正文大小。
         */
        auto SendRequest(const RequestParameters& Parameters)
            -> Net::awaitable<ClientResponse>
        {
            auto Ex = MemberExecutor;

            Net::ip::tcp::resolver Resolver{Ex};
            Net::steady_timer ResolveTimer{Ex, MemberTimeout};

            boost::system::error_code ResolveEc;
            using namespace Net::experimental::awaitable_operators;
            auto Resolution = co_await (
                Resolver.async_resolve(Parameters.Host, Parameters.Port,
                    Net::redirect_error(Net::use_awaitable, ResolveEc))
                || ResolveTimer.async_wait(Net::use_awaitable));
            if (std::holds_alternative<std::monostate>(Resolution))
            {
                throw boost::system::system_error(Net::error::timed_out);
            }
            if (ResolveEc)
            {
                throw boost::system::system_error(ResolveEc);
            }
            auto Results = std::get<0>(std::move(Resolution));

            Beast::tcp_stream Stream{Ex};
            Stream.expires_after(MemberTimeout);
            co_await Stream.async_connect(
                Results, Net::use_awaitable);

            Http::request<Http::string_body> Request{
                Parameters.Method, Parameters.Target, 11};
            Request.set(Http::field::host, Parameters.Host);
            Request.set(Http::field::user_agent, "wan/web-client");
            Request.set(Http::field::connection, "close");

            if (!Parameters.Body.empty())
            {
                Request.set(Http::field::content_type,
                    "application/x-www-form-urlencoded");
                Request.body() = Parameters.Body;
            }

            Request.prepare_payload();
            Stream.expires_after(MemberTimeout);
            co_await Http::async_write(
                Stream, Request, Net::use_awaitable);

            Beast::flat_buffer Buffer;
            Http::response_parser<Http::string_body> Parser;
            Parser.header_limit(MemberMaxResponseHeaderSize);
            Parser.body_limit(MemberMaxResponseBodySize);
            Stream.expires_after(MemberTimeout);
            co_await Http::async_read(
                Stream, Buffer, Parser, Net::use_awaitable);
            auto Resp = Parser.release();

            boost::system::error_code Ec;
            Ec = Stream.socket().shutdown(
                Net::ip::tcp::socket::shutdown_both, Ec);

            ClientResponse Result;
            Result.Status = Resp.result();
            Result.Body = std::move(Resp.body());
            Result.Headers = std::move(Resp.base());

            co_return Result;
        }

        /** @brief DNS、连接和 HTTP 异步操作使用的执行器。 */
        Net::any_io_executor MemberExecutor;
        /** @brief 每个 DNS、连接、写入和读取阶段的超时。 */
        std::chrono::seconds MemberTimeout{30};
        /** @brief Beast 响应解析器允许的最大响应头字节数。 */
        std::size_t MemberMaxResponseHeaderSize = 64 * 1024;
        /** @brief Beast 响应解析器允许的最大正文字节数。 */
        std::size_t MemberMaxResponseBodySize = std::size_t{64} * 1024 * 1024;

        /**
         * @brief 校验超时和响应大小限制。
         * @throws std::invalid_argument 超时非正或不可表示，或任一大小限制为零时抛出。
         */
        void ValidateConfiguration() const
        {
            const auto MaximumTimerSeconds = std::chrono::duration_cast<
                std::chrono::seconds>(
                std::chrono::steady_clock::duration::max());
            if (MemberTimeout <= std::chrono::seconds::zero()
                || MemberTimeout > MaximumTimerSeconds)
            {
                throw std::invalid_argument(
                    "HTTP client timeout must be positive and representable");
            }
            if (MemberMaxResponseHeaderSize == 0
                || MemberMaxResponseBodySize == 0)
            {
                throw std::invalid_argument(
                    "HTTP client response limits must be positive");
            }
        }
    };
}
