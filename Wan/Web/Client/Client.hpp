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
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/beast.hpp>
#include <boost/url.hpp>

#include <cstdio>
#include <chrono>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <openssl/ssl.h>

#include "Wan/Fault/Fault.hpp"
#include "Wan/Web/Client/TlsClient.hpp"

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
        /** @brief HTTPS 客户端的额外 CA 信任根。 */
        HttpClientTlsConfig Tls;
        /** @brief 是否允许 HTTPS 重定向降级到明文 HTTP。 */
        bool AllowInsecureRedirects = false;
    };

    namespace Detail
    {
        /** @brief 解析后的绝对 HTTP 或 HTTPS URL。 */
        struct ParsedHttpUrl
        {
            /** @brief HTTP 或 HTTPS scheme。 */
            std::string Scheme;
            /** @brief 目标主机名或 IP 地址。 */
            std::string Host;
            /** @brief 显式或按协议推导的服务端口。 */
            std::string Port;
            /** @brief HTTP Host 请求头值。 */
            std::string HostHeader;
            /** @brief URI authority。 */
            std::string Authority;
            /** @brief 请求目标路径和查询串。 */
            std::string Target;
        };

        /**
         * @brief 解析绝对 HTTP 或 HTTPS URL。
         * @param Value 要解析的 URL。
         * @return 支持的 URL 组件；无效或不支持的 URI 返回空值。
         */
        [[nodiscard]] inline auto ParseHttpUrl(std::string_view Value)
            -> std::optional<ParsedHttpUrl>
        {
            const auto Parsed = boost::urls::parse_uri_reference(Value);
            if (!Parsed || !Parsed->has_scheme()
                || !Parsed->has_authority()
                || Parsed->has_userinfo())
            {
                return std::nullopt;
            }

            ParsedHttpUrl Result;
            if (Parsed->scheme_id() == boost::urls::scheme::http)
            {
                Result.Scheme = "http";
            }
            else if (Parsed->scheme_id() == boost::urls::scheme::https)
            {
                Result.Scheme = "https";
            }
            else
            {
                return std::nullopt;
            }

            const auto HostType = Parsed->host_type();
            if (HostType == boost::urls::host_type::none
                || HostType == boost::urls::host_type::ipvfuture)
            {
                return std::nullopt;
            }
            if (HostType == boost::urls::host_type::ipv4
                || HostType == boost::urls::host_type::ipv6)
            {
                Result.Host = std::string(Parsed->host_address());
            }
            else
            {
                Result.Host = std::string(Parsed->host());
            }

            if (Result.Host.empty()
                || (Parsed->has_port() && Parsed->port().empty()))
            {
                return std::nullopt;
            }
            Result.Port = Parsed->port().empty()
                ? (Result.Scheme == "https" ? "443" : "80")
                : std::string(Parsed->port());
            Result.Authority = std::string(Parsed->encoded_authority());
            Result.HostHeader = Result.Authority;
            Result.Target = std::string(Parsed->encoded_target());
            if (Result.Target.empty() || Result.Target.front() == '?')
            {
                Result.Target.insert(0, "/");
            }
            return Result;
        }
    }

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
            , MemberTlsConfig(std::move(Config.Tls))
            , MemberAllowInsecureRedirects(Config.AllowInsecureRedirects)
            , MemberTlsContextCache(std::make_shared<TlsContextCache>())
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
            return DoRequest(
                RequestParameters{Http::verb::get, "http",
                    std::string(Host), std::string(Port),
                    std::string(Host), BuildAuthority(Host, Port),
                    std::string(Target), {}},
                MaxRedirects);
        }

        /**
         * @brief 按绝对 HTTP 或 HTTPS URL 发送 GET 请求。
         * @param Url 绝对 URL。
         * @param MaxRedirects 最大重定向次数。
         * @return HTTP 响应。
         * @throws std::invalid_argument URL scheme、authority 或重定向次数无效时抛出。
         * @throws boost::system::system_error 网络或 TLS 验证失败时抛出。
         */
        [[nodiscard]] auto Get(std::string_view Url, int MaxRedirects = 5)
            -> Net::awaitable<ClientResponse>
        {
            return DoAbsoluteUrlRequest(Http::verb::get, std::string(Url), {},
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
            return DoRequest(
                RequestParameters{Http::verb::post, "http",
                    std::string(Host), std::string(Port),
                    std::string(Host), BuildAuthority(Host, Port),
                    std::string(Target),
                    std::string(Body)},
                MaxRedirects);
        }

        /**
         * @brief 按绝对 HTTP 或 HTTPS URL 发送 POST 请求。
         * @param Url 绝对 URL。
         * @param Body 请求正文。
         * @param MaxRedirects 最大重定向次数。
         * @return HTTP 响应。
         * @throws std::invalid_argument URL scheme、authority 或重定向次数无效时抛出。
         * @throws boost::system::system_error 网络或 TLS 验证失败时抛出。
         */
        [[nodiscard]] auto Post(std::string_view Url, std::string_view Body,
            int MaxRedirects = 5) -> Net::awaitable<ClientResponse>
        {
            return DoAbsoluteUrlRequest(Http::verb::post, std::string(Url),
                std::string(Body), MaxRedirects);
        }

    private:
        /** @brief 一次 HTTP 请求的目标和正文参数。 */
        struct RequestParameters
        {
            /** @brief HTTP 方法。 */
            Http::verb Method;
            /** @brief 当前请求协议。 */
            std::string Scheme;
            /** @brief 目标主机名。 */
            std::string Host;
            /** @brief 目标服务端口。 */
            std::string Port;
            /** @brief HTTP Host 请求头值。 */
            std::string HostHeader;
            /** @brief 用于解析相对重定向的 URI authority。 */
            std::string Authority;
            /** @brief 请求目标路径和查询串。 */
            std::string Target;
            /** @brief 请求正文。 */
            std::string Body;
        };

        /**
         * @brief 解析 URL 并发送 GET 或 POST 请求。
         * @param Method HTTP 请求方法。
         * @param Url 已复制到当前协程的绝对 URL。
         * @param Body 请求正文；GET 请求传入空字符串。
         * @param MaxRedirects 允许跟随的最大重定向次数。
         * @return 远端 HTTP 响应。
         * @throws std::invalid_argument URL 无效或重定向次数为负数时抛出。
         */
        auto DoAbsoluteUrlRequest(Http::verb Method, std::string Url,
            std::string Body, int MaxRedirects)
            -> Net::awaitable<ClientResponse>
        {
            const auto Parsed = Detail::ParseHttpUrl(Url);
            if (!Parsed)
            {
                throw std::invalid_argument(
                    "HTTP client URL must be an absolute http or https URL");
            }
            co_return co_await DoRequest(
                RequestParameters{Method, Parsed->Scheme,
                    Parsed->Host, Parsed->Port, Parsed->HostHeader,
                    Parsed->Authority, Parsed->Target, std::move(Body)},
                MaxRedirects);
        }

        /**
         * @brief 根据主机和显式端口构造重定向解析用 authority。
         * @param Host 主机名或 IP 地址。
         * @param Port 服务端口。
         * @return URI authority 字符串。
         */
        [[nodiscard]] static auto BuildAuthority(std::string_view Host,
            std::string_view Port) -> std::string
        {
            std::string Authority{Host};
            if (Host.find(':') != std::string_view::npos
                && !(Host.starts_with('[') && Host.ends_with(']')))
            {
                Authority = "[" + Authority + "]";
            }
            Authority += ":";
            Authority += Port;
            return Authority;
        }

        /**
         * @brief 构造包含 scheme、authority 和 target 的绝对 URL。
         * @param Parameters 请求的 scheme、authority 和 target。
         * @return 供重定向解析使用的绝对 URL。
         */
        [[nodiscard]] static auto AbsoluteUrl(
            const RequestParameters& Parameters) -> std::string
        {
            return Parameters.Scheme + "://" + Parameters.Authority
                + Parameters.Target;
        }

        /**
         * @brief 按 RFC 3986 将重定向目标解析到当前请求 URL 上。
         * @param Parameters 当前请求的 URL 组件。
         * @param Location 响应中的 Location 字段值。
         * @return 支持的目标 URL；不支持或格式无效时返回空值。
         */
        [[nodiscard]] static auto ResolveRedirect(
            const RequestParameters& Parameters, std::string_view Location)
            -> std::optional<Detail::ParsedHttpUrl>
        {
            const auto BaseUrl = AbsoluteUrl(Parameters);
            const auto Base = boost::urls::parse_uri(BaseUrl);
            const auto Reference = boost::urls::parse_uri_reference(Location);
            if (!Base || !Reference)
            {
                return std::nullopt;
            }

            boost::urls::url Resolved;
            const auto Result = boost::urls::resolve(
                *Base, *Reference, Resolved);
            if (!Result)
            {
                return std::nullopt;
            }
            return Detail::ParseHttpUrl(Resolved.buffer());
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
                        const auto Parsed = ResolveRedirect(Parameters,
                            std::string_view{LocationValue.data(),
                                LocationValue.size()});
                        if (!Parsed)
                        {
                            co_return Resp;
                        }
                        if (Parameters.Scheme == "https"
                            && Parsed->Scheme == "http"
                            && !MemberAllowInsecureRedirects)
                        {
                            co_return Resp;
                        }
                        Parameters.Scheme = Parsed->Scheme;
                        Parameters.Host = Parsed->Host;
                        Parameters.Port = Parsed->Port;
                        Parameters.HostHeader = Parsed->HostHeader;
                        Parameters.Authority = Parsed->Authority;
                        Parameters.Target = Parsed->Target;
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

        /** @brief TLS 客户端流类型。 */
        using TlsStream = Net::ssl::stream<Beast::tcp_stream>;

        /**
         * @brief 取得 TLS 流或普通流中的 TCP 层。
         * @tparam Stream 普通 TCP 流或 TLS 流类型。
         * @param Value 活动网络流。
         * @return 底层 Beast TCP 流。
         */
        template <typename Stream>
        [[nodiscard]] static auto TcpLayer(Stream& Value) noexcept
            -> Beast::tcp_stream&
        {
            if constexpr (std::is_same_v<Stream, TlsStream>)
            {
                return Value.next_layer();
            }
            else
            {
                return Value;
            }
        }

        /**
         * @brief 设置 TCP 传输层的下一次操作超时。
         * @tparam Stream 普通 TCP 流或 TLS 流类型。
         * @param Value 活动网络流。
         * @param Timeout 超时时长；零表示不设超时。
         */
        template <typename Stream>
        static void SetStreamTimeout(Stream& Value,
            std::chrono::seconds Timeout)
        {
            auto& Tcp = TcpLayer(Value);
            if (Timeout.count() > 0)
            {
                Tcp.expires_after(Timeout);
            }
            else
            {
                Tcp.expires_never();
            }
        }

        /**
         * @brief 取得共享的、延迟创建的 HTTPS 客户端上下文。
         * @return 已加载系统信任根和自定义 CA 的 TLS 上下文。
         */
        [[nodiscard]] auto GetTlsContext() const
            -> std::shared_ptr<Net::ssl::context>
        {
            std::lock_guard<std::mutex> Lock{
                MemberTlsContextCache->Mutex};
            if (!MemberTlsContextCache->Context)
            {
                MemberTlsContextCache->Context =
                    Detail::CreateClientTlsContext(MemberTlsConfig);
            }
            return MemberTlsContextCache->Context;
        }

        /**
         * @brief 通过已连接的普通或 TLS 流发送 HTTP/1.1 请求。
         * @tparam Stream 普通 TCP 流或 TLS 流类型。
         * @param Connection 已完成 TCP 或 TLS 握手的连接。
         * @param Parameters 请求方法、头部目标、正文和 Host 信息。
         * @return 收到的 HTTP 响应。
         */
        template <typename Stream>
        auto SendHttpRequest(Stream& Connection,
            const RequestParameters& Parameters)
            -> Net::awaitable<ClientResponse>
        {
            Http::request<Http::string_body> Request{
                Parameters.Method, Parameters.Target, 11};
            Request.set(Http::field::host, Parameters.HostHeader);
            Request.set(Http::field::user_agent, "wan/web-client");
            Request.set(Http::field::connection, "close");

            if (!Parameters.Body.empty())
            {
                Request.set(Http::field::content_type,
                    "application/x-www-form-urlencoded");
                Request.body() = Parameters.Body;
            }

            Request.prepare_payload();
            SetStreamTimeout(Connection, MemberTimeout);
            co_await Http::async_write(Connection, Request,
                Net::use_awaitable);

            Beast::flat_buffer Buffer;
            Http::response_parser<Http::string_body> Parser;
            Parser.header_limit(MemberMaxResponseHeaderSize);
            Parser.body_limit(MemberMaxResponseBodySize);
            SetStreamTimeout(Connection, MemberTimeout);
            co_await Http::async_read(Connection, Buffer, Parser,
                Net::use_awaitable);
            auto Resp = Parser.release();

            if constexpr (std::is_same_v<Stream, TlsStream>)
            {
                SetStreamTimeout(Connection, MemberTimeout);
                boost::system::error_code ShutdownEc;
                co_await Connection.async_shutdown(
                    Net::redirect_error(Net::use_awaitable, ShutdownEc));
                if (ShutdownEc
                    && ShutdownEc != Net::error::operation_aborted
                    && ShutdownEc != Net::error::eof
                    && ShutdownEc != Net::ssl::error::stream_truncated)
                {
                    throw boost::system::system_error(
                        boost::system::error_code{
                            Wan::Fault::Code::TlsShutdownFailed},
                        ShutdownEc.message());
                }
            }
            else
            {
                boost::system::error_code Ec;
                TcpLayer(Connection).socket().shutdown(
                    Net::ip::tcp::socket::shutdown_both, Ec);
            }

            ClientResponse Result;
            Result.Status = Resp.result();
            Result.Body = std::move(Resp.body());
            Result.Headers = std::move(Resp.base());
            co_return Result;
        }

        /**
         * @brief 解析地址并发送一次 HTTP 或 HTTPS 请求。
         * @param Parameters 本次请求的参数。
         * @return 收到的状态码、正文和响应头。
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

            if (Parameters.Scheme == "https")
            {
                auto Context = GetTlsContext();
                TlsStream Stream{Ex, *Context};
                SetStreamTimeout(Stream, MemberTimeout);
                co_await TcpLayer(Stream).async_connect(
                    Results, Net::use_awaitable);

                boost::system::error_code AddressEc;
                (void)Net::ip::make_address(Parameters.Host, AddressEc);
                if (AddressEc)
                {
                    if (SSL_set_tlsext_host_name(Stream.native_handle(),
                            Parameters.Host.c_str()) != 1)
                    {
                        throw boost::system::system_error(
                            boost::system::error_code{
                                Wan::Fault::Code::TlsHandshakeFailed},
                            "unable to set TLS server name");
                    }
                }
                Stream.set_verify_mode(Net::ssl::verify_peer);
                Stream.set_verify_callback(
                    Net::ssl::host_name_verification(Parameters.Host));

                SetStreamTimeout(Stream, MemberTimeout);
                boost::system::error_code HandshakeEc;
                co_await Stream.async_handshake(
                    Net::ssl::stream_base::client,
                    Net::redirect_error(Net::use_awaitable, HandshakeEc));
                if (HandshakeEc)
                {
                    throw boost::system::system_error(
                        boost::system::error_code{
                            Wan::Fault::Code::TlsHandshakeFailed},
                        HandshakeEc.message());
                }
                co_return co_await SendHttpRequest(Stream, Parameters);
            }

            Beast::tcp_stream Stream{Ex};
            Stream.expires_after(MemberTimeout);
            co_await Stream.async_connect(Results, Net::use_awaitable);
            co_return co_await SendHttpRequest(Stream, Parameters);
        }

        /** @brief 共享并延迟创建 HTTPS SSL 上下文的状态。 */
        struct TlsContextCache
        {
            /** @brief 串行化 TLS 上下文的首次创建。 */
            std::mutex Mutex;
            /** @brief 已加载系统信任根和自定义 CA 的共享上下文。 */
            std::shared_ptr<Net::ssl::context> Context;
        };

        /** @brief DNS、连接和 HTTP 异步操作使用的执行器。 */
        Net::any_io_executor MemberExecutor;
        /** @brief 每个 DNS、连接、写入和读取阶段的超时。 */
        std::chrono::seconds MemberTimeout{30};
        /** @brief Beast 响应解析器允许的最大响应头字节数。 */
        std::size_t MemberMaxResponseHeaderSize = 64 * 1024;
        /** @brief Beast 响应解析器允许的最大正文字节数。 */
        std::size_t MemberMaxResponseBodySize = std::size_t{64} * 1024 * 1024;
        /** @brief 系统信任根和附加 CA 配置。 */
        HttpClientTlsConfig MemberTlsConfig;
        /** @brief 是否允许 HTTPS 重定向降级到 HTTP。 */
        bool MemberAllowInsecureRedirects = false;
        /** @brief 多个并发请求共享的延迟 TLS 上下文。 */
        std::shared_ptr<TlsContextCache> MemberTlsContextCache;

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
