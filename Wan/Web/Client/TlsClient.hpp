/**
 * @file TlsClient.hpp
 * @brief HTTPS 客户端的信任根配置和 TLS 上下文构造。
 */
#pragma once

#include <boost/asio/ssl.hpp>

#include <filesystem>
#include <climits>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#ifdef _WIN32
#include <windows.h>
#include <wincrypt.h>
#endif

namespace Wan::Web
{
    /**
     * @struct HttpClientTlsConfig
     * @brief HTTPS 客户端使用的额外受信 CA。
     * @details 系统根证书始终加载；这些 CA 会追加到系统信任根。
     */
    struct HttpClientTlsConfig
    {
        /** @brief PEM 格式 CA 文件路径；可配置多个文件。 */
        std::vector<std::filesystem::path> CaCertificateFiles;
        /** @brief PEM 格式 CA 内容；每项可包含一个或多个证书。 */
        std::vector<std::string> CaCertificatePem;
    };

    namespace Detail
    {
        /**
         * @brief 将 CA 证书加入 BoringSSL 信任库。
         * @param Store 目标 X.509 信任库。
         * @param Certificate 要加入的证书。
         * @throws std::runtime_error 证书无法加入且并非重复证书时抛出。
         */
        inline void AddTrustedCertificate(X509_STORE* Store, X509* Certificate)
        {
            if (X509_STORE_add_cert(Store, Certificate) == 1)
            {
                return;
            }
            const auto Error = ERR_peek_last_error();
            if (Error != 0
                && ERR_GET_REASON(Error)
                    == X509_R_CERT_ALREADY_IN_HASH_TABLE)
            {
                ERR_clear_error();
                return;
            }
            throw std::runtime_error(
                "unable to add a trusted CA certificate");
        }

#ifdef _WIN32
        /**
         * @brief 将一个 Windows 系统根证书库导入 BoringSSL。
         * @param StoreLocation 当前用户或本地计算机证书库位置。
         * @param TrustStore BoringSSL 的 X.509 信任库。
         * @param Seen 用于跳过当前用户和本地计算机库之间的重复证书。
         * @return 新导入的证书数量。
         */
        inline auto ImportWindowsRootStore(DWORD StoreLocation,
            X509_STORE* TrustStore, std::unordered_set<std::string>& Seen)
            -> std::size_t
        {
            const DWORD Flags = StoreLocation
                | CERT_STORE_OPEN_EXISTING_FLAG
                | CERT_STORE_READONLY_FLAG;
            HCERTSTORE Store = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0,
                static_cast<HCRYPTPROV_LEGACY>(0), Flags, L"ROOT");
            if (!Store)
            {
                throw std::runtime_error(
                    "unable to open the Windows ROOT certificate store");
            }

            std::size_t Imported = 0;
            PCCERT_CONTEXT Certificate = nullptr;
            try
            {
                while ((Certificate = CertEnumCertificatesInStore(
                            Store, Certificate)) != nullptr)
                {
                    std::string Encoded(
                        reinterpret_cast<const char*>(
                            Certificate->pbCertEncoded),
                        Certificate->cbCertEncoded);
                    if (!Seen.emplace(std::move(Encoded)).second)
                    {
                        continue;
                    }

                    const unsigned char* Data = Certificate->pbCertEncoded;
                    X509* Parsed = d2i_X509(nullptr, &Data,
                        static_cast<long>(Certificate->cbCertEncoded));
                    if (!Parsed)
                    {
                        throw std::runtime_error(
                            "unable to decode a Windows ROOT certificate");
                    }
                    try
                    {
                        AddTrustedCertificate(TrustStore, Parsed);
                    }
                    catch (...)
                    {
                        X509_free(Parsed);
                        throw;
                    }
                    X509_free(Parsed);
                    ++Imported;
                }

                if (GetLastError() != CRYPT_E_NOT_FOUND)
                {
                    throw std::runtime_error(
                        "unable to enumerate the Windows ROOT certificate store");
                }
            }
            catch (...)
            {
                if (Certificate)
                {
                    CertFreeCertificateContext(Certificate);
                }
                CertCloseStore(Store, 0);
                throw;
            }

            CertCloseStore(Store, 0);
            return Imported;
        }

        /**
         * @brief 从当前用户和本地计算机的 Windows ROOT 证书库导入系统信任根。
         * @param Context 要加载信任根的 TLS 上下文。
         * @throws std::runtime_error Windows ROOT 证书库无法打开、读取或为空时抛出。
         */
        inline void LoadSystemTrustRoots(
            boost::asio::ssl::context& Context)
        {
            X509_STORE* TrustStore = SSL_CTX_get_cert_store(
                Context.native_handle());
            if (!TrustStore)
            {
                throw std::runtime_error(
                    "unable to access the TLS certificate trust store");
            }

            std::unordered_set<std::string> Seen;
            const auto CurrentUser = ImportWindowsRootStore(
                CERT_SYSTEM_STORE_CURRENT_USER, TrustStore, Seen);
            const auto LocalMachine = ImportWindowsRootStore(
                CERT_SYSTEM_STORE_LOCAL_MACHINE, TrustStore, Seen);
            if (CurrentUser + LocalMachine == 0)
            {
                throw std::runtime_error(
                    "the Windows ROOT certificate stores are empty");
            }
        }
#else
        /**
         * @brief 从 BoringSSL 默认 CA 路径加载系统信任根。
         * @param Context 要加载信任根的 TLS 上下文。
         */
        inline void LoadSystemTrustRoots(
            boost::asio::ssl::context& Context)
        {
            Context.set_default_verify_paths();
        }
#endif

        /**
         * @brief 将一项或多项 PEM CA 证书加入 TLS 信任库。
         * @param Context 要加载 CA 证书的 TLS 上下文。
         * @param Pem 一个或多个 PEM 编码证书。
         * @throws std::invalid_argument PEM 为空、不可表示或不包含有效证书时抛出。
         * @throws std::runtime_error 读取器创建或证书导入失败时抛出。
         */
        inline void LoadPemTrustRoots(boost::asio::ssl::context& Context,
            const std::string& Pem)
        {
            if (Pem.empty()
                || Pem.size() > static_cast<std::size_t>(INT_MAX))
            {
                throw std::invalid_argument(
                    "CA PEM data must be non-empty and representable");
            }

            BIO* Bio = BIO_new_mem_buf(Pem.data(),
                static_cast<int>(Pem.size()));
            if (!Bio)
            {
                throw std::runtime_error(
                    "unable to create a CA PEM reader");
            }

            X509_STORE* TrustStore = SSL_CTX_get_cert_store(
                Context.native_handle());
            bool LoadedCertificate = false;
            try
            {
                for (;;)
                {
                    X509* Certificate = PEM_read_bio_X509(
                        Bio, nullptr, nullptr, nullptr);
                    if (!Certificate)
                    {
                        break;
                    }
                    LoadedCertificate = true;
                    try
                    {
                        AddTrustedCertificate(TrustStore, Certificate);
                    }
                    catch (...)
                    {
                        X509_free(Certificate);
                        throw;
                    }
                    X509_free(Certificate);
                }

                const auto ParseError = ERR_peek_last_error();
                if (!LoadedCertificate
                    || (ParseError != 0
                        && ERR_GET_REASON(ParseError)
                            != PEM_R_NO_START_LINE))
                {
                    throw std::invalid_argument(
                        "CA PEM data does not contain valid certificates");
                }
                ERR_clear_error();
            }
            catch (...)
            {
                BIO_free(Bio);
                ERR_clear_error();
                throw;
            }
            BIO_free(Bio);
        }

        /**
         * @brief 创建并配置默认验证对端的 HTTPS 客户端上下文。
         * @param Config 附加 CA 文件与 PEM 配置。
         * @return 加载系统信任根和附加 CA 的 TLS 上下文。
         * @throws std::invalid_argument 附加 PEM 数据无效时抛出。
         * @throws std::runtime_error 系统根、CA 文件或 TLS 上下文无法加载时抛出。
         */
        [[nodiscard]] inline auto CreateClientTlsContext(
            const HttpClientTlsConfig& Config)
            -> std::shared_ptr<boost::asio::ssl::context>
        {
            auto Context = std::make_shared<boost::asio::ssl::context>(
                boost::asio::ssl::context::tls_client);
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
            Context->set_verify_mode(boost::asio::ssl::verify_peer);
            LoadSystemTrustRoots(*Context);

            for (const auto& CaFile : Config.CaCertificateFiles)
            {
                if (CaFile.empty())
                {
                    throw std::invalid_argument(
                        "CA certificate file path cannot be empty");
                }
                Context->load_verify_file(CaFile.string());
            }
            for (const auto& CaPem : Config.CaCertificatePem)
            {
                LoadPemTrustRoots(*Context, CaPem);
            }
            return Context;
        }
    }
}
