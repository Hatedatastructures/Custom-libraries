/**
 * @file TlsServer.hpp
 * @brief 带有 TLS 传输的 Web 服务器。
 */
#pragma once

#include <boost/asio/ssl.hpp>

#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

#include <openssl/ssl.h>

#include "Wan/Fault/Fault.hpp"
#include "Wan/Web/Server/Server.hpp"

namespace Wan::Web
{
    /**
     * @struct TlsServerConfig
     * @brief TLS 服务端证书链和私钥来源。
     * @details 每项凭据必须从 PEM 文件或内存 PEM 中选择且只选择一种。
     */
    struct TlsServerConfig
    {
        /** @brief PEM 格式的证书链文件路径。 */
        std::filesystem::path CertificateChainFile;
        /** @brief PEM 格式的证书链内容。 */
        std::string CertificateChainPem;
        /** @brief PEM 格式的私钥文件路径。 */
        std::filesystem::path PrivateKeyFile;
        /** @brief PEM 格式的私钥内容。 */
        std::string PrivateKeyPem;
        /** @brief 解密私钥时获取口令的回调。 */
        std::function<std::string()> PrivateKeyPasswordCallback;
    };

    namespace Detail
    {
        /**
         * @brief 创建并加载服务端 TLS 上下文。
         * @param Config 证书、私钥及解密口令回调。
         * @return 已设置 TLS 1.2 最低版本和服务端凭据的上下文。
         */
        [[nodiscard]] inline auto CreateServerTlsContext(
            const TlsServerConfig& Config)
            -> std::shared_ptr<boost::asio::ssl::context>
        {
            const bool HasCertificateFile =
                !Config.CertificateChainFile.empty();
            const bool HasCertificatePem = !Config.CertificateChainPem.empty();
            const bool HasKeyFile = !Config.PrivateKeyFile.empty();
            const bool HasKeyPem = !Config.PrivateKeyPem.empty();
            if (HasCertificateFile == HasCertificatePem
                || HasKeyFile == HasKeyPem)
            {
                throw std::invalid_argument(
                    "TLS certificate chain and private key must each have exactly one PEM source");
            }

            auto Context = std::make_shared<boost::asio::ssl::context>(
                boost::asio::ssl::context::tls_server);
            Context->set_options(
                boost::asio::ssl::context::default_workarounds
                | boost::asio::ssl::context::no_sslv2
                | boost::asio::ssl::context::no_sslv3);
            if (SSL_CTX_set_min_proto_version(
                    Context->native_handle(), TLS1_2_VERSION) != 1)
            {
                throw std::runtime_error(
                    "unable to require TLS 1.2 or newer");
            }

            if (Config.PrivateKeyPasswordCallback)
            {
                Context->set_password_callback(
                    [Callback = Config.PrivateKeyPasswordCallback](
                        std::size_t,
                        boost::asio::ssl::context::password_purpose)
                    {
                        return Callback();
                    });
            }

            try
            {
                if (HasCertificateFile)
                {
                    Context->use_certificate_chain_file(
                        Config.CertificateChainFile.string());
                }
                else
                {
                    Context->use_certificate_chain(boost::asio::buffer(
                        Config.CertificateChainPem));
                }
            }
            catch (const std::exception& Error)
            {
                throw boost::system::system_error(
                    boost::system::error_code{
                        Wan::Fault::Code::SslCertLoadFailed}, Error.what());
            }

            try
            {
                if (HasKeyFile)
                {
                    Context->use_private_key_file(
                        Config.PrivateKeyFile.string(),
                        boost::asio::ssl::context::pem);
                }
                else
                {
                    Context->use_private_key(boost::asio::buffer(
                        Config.PrivateKeyPem),
                        boost::asio::ssl::context::pem);
                }
                if (SSL_CTX_check_private_key(Context->native_handle()) != 1)
                {
                    throw std::runtime_error(
                        "TLS certificate and private key do not match");
                }
            }
            catch (const std::exception& Error)
            {
                throw boost::system::system_error(
                    boost::system::error_code{
                        Wan::Fault::Code::SslKeyLoadFailed}, Error.what());
            }

            return Context;
        }
    }

    /**
     * @class TlsServer
     * @brief 使用 HTTPS 传输的 Web 服务器。
     * @details 路由、指标和生命周期接口继承自 `Server`，请求处理流程共用。
     */
    class TlsServer final : public Server
    {
    public:
        /**
         * @brief 创建 TLS 服务器。
         * @param ServerConfiguration HTTP、监听与资源限制配置。
         * @param TlsConfiguration 证书链、私钥和私钥口令回调。
         * @throws std::invalid_argument 证书或私钥来源缺失或互相冲突时抛出。
         * @throws boost::system::system_error 证书或私钥无法加载时抛出。
         */
        TlsServer(ServerConfig ServerConfiguration,
            const TlsServerConfig& TlsConfiguration)
            : Server(std::move(ServerConfiguration),
                Detail::CreateServerTlsContext(TlsConfiguration))
        {
        }
    };
}
