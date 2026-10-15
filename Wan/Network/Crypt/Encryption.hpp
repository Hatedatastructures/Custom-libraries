/**
 * @file Encryption.hpp
 * @brief 基于 Crypto++ 的加密、密钥派生和摘要工具。
 */
/** @brief 启用本模块使用的 Crypto++ 弱算法命名空间。 */
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#pragma once
// 基于 Crypto++ 的加解密封装。

#include <osrng.h>
#include <hex.h>
#include <filters.h>
#include <sha.h>
#include <md5.h>
#include <hmac.h>
#include <pwdbased.h>
#include <gcm.h>
#include <aes.h>
#include <modes.h>
#include <twofish.h>
#include <secblock.h>
#include <base64.h>
#include <rsa.h>
#include <osrng.h>
#include <pssr.h>
#include <files.h>
#include <elgamal.h>
#include <nbtheory.h>
#include <integer.h>
#include <cryptlib.h>
#include <filters.h>

#include <string>
#include <vector>
#include <stdexcept>
#include <memory>
#include <sstream>
#include <utility>
#include <cstring>
#include <limits>
#include <string_view>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

namespace Encryption
{

    /** @brief Crypto++ 字节数据使用的无符号字节类型。 */
    using CryptoByte = unsigned char;

    /**
     * @brief 管理 Crypto++ 随机数池。
     */
    class AeonRandom
    {
    public:
        /** @brief 创建并播种随机数池。 */
        AeonRandom() = default;

        /**
         * @brief 获取底层随机数池。
         * @return 指向当前 `AeonRandom` 所拥有随机数池的可写引用。
         * @note 引用不得超过当前 `AeonRandom` 对象的生命周期。
         */
        CryptoPP::AutoSeededRandomPool &Pool() { return MemberRng; }

    private:
        /** @brief 由构造函数播种并管理的 Crypto++ 随机数池。 */
        CryptoPP::AutoSeededRandomPool MemberRng;
    };

    /**
     * @brief 将字节序列编码为小写十六进制字符串。
     * @param Data 要编码的字节序列。
     * @param Len 要编码的字节数。
     * @return 编码后的十六进制文本。
     * @pre `Data` 至少指向 `Len` 个可读字节；`Len` 为零时不读取数据。
     */
    static inline std::string ToHex(const CryptoByte *Data, size_t Len)
    {
        std::string Out;
        CryptoPP::HexEncoder Encoder(new CryptoPP::StringSink(Out), false);
        Encoder.Put(Data, Len);
        Encoder.MessageEnd();
        return Out;
    }
    /**
     * @brief 将十六进制文本解码为原始字节串。
     * @param Hex 要解码的十六进制文本。
     * @return 解码得到的原始字节串。
     * @note Crypto++ 解码异常会向调用方传播。
     */
    static inline std::string FromHexToRaw(const std::string &Hex)
    {
        std::string Out;
        CryptoPP::StringSource Ss(Hex, true, new CryptoPP::HexDecoder(new CryptoPP::StringSink(Out)));
        return Out;
    }
    /**
     * @brief 将原始字节串编码为不带换行的 Base64 文本。
     * @param Raw 要编码的原始字节串。
     * @return Base64 编码文本。
     */
    static inline std::string ToBase64(const std::string &Raw)
    {
        std::string Out;
        CryptoPP::StringSource Ss(Raw, true, new CryptoPP::Base64Encoder(new CryptoPP::StringSink(Out), false));
        return Out;
    }
    /**
     * @brief 将 Base64 文本解码为原始字节串。
     * @param B64 要解码的 Base64 文本。
     * @return 解码得到的原始字节串。
     * @note Crypto++ 解码异常会向调用方传播。
     */
    static inline std::string FromBase64(const std::string &B64)
    {
        std::string Out;
        CryptoPP::StringSource Ss(B64, true, new CryptoPP::Base64Decoder(new CryptoPP::StringSink(Out)));
        return Out;
    }

    /**
     * @brief 使用 PBKDF2-HMAC-SHA256 从密码派生密钥。
     * @param Pass 密码。
     * @param Salt salt 字节数组。
     * @param SaltLen salt 长度。
     * @param KeyLen 派生密钥长度。
     * @param Iterations 迭代次数，默认值为 200000。
     * @return 派生出的密钥字节。
     * @throws std::invalid_argument salt 为空或长度、迭代次数为零时抛出。
     */
    static inline std::string DeriveKeyFromPassword(const std::string &Pass, const CryptoByte *Salt, size_t SaltLen, size_t KeyLen, unsigned Iterations = 200000)
    {
        if (Salt == nullptr || SaltLen == 0 || KeyLen == 0 || Iterations == 0)
        {
            throw std::invalid_argument(
                "PBKDF2 requires non-empty salt and output with positive iterations");
        }
        std::string Key;
        Key.resize(KeyLen);
        CryptoPP::PKCS5_PBKDF2_HMAC<CryptoPP::SHA256> Pbkdf;
        Pbkdf.DeriveKey(reinterpret_cast<CryptoByte *>(Key.data()), KeyLen, 0,
            reinterpret_cast<const CryptoByte *>(Pass.data()), Pass.size(),
            Salt, SaltLen, Iterations);
        return Key;
    }

    /**
     * @brief 比较两个等长字符串的内容，避免按首个差异提前返回。
     * @param Left 左侧字符串。
     * @param Right 右侧字符串。
     * @return 两个字符串长度和内容均相同时为 `true`。
     */
    [[nodiscard]] inline auto ConstantTimeEqual(
        std::string_view Left, std::string_view Right) noexcept -> bool
    {
        if (Left.size() != Right.size())
        {
            return false;
        }

        unsigned char Difference = 0;
        for (std::size_t I = 0; I < Left.size(); ++I)
        {
            Difference |= static_cast<unsigned char>(
                static_cast<unsigned char>(Left[I])
                ^ static_cast<unsigned char>(Right[I]));
        }
        return Difference == 0;
    }

    /**
     * @brief 以小端字节序追加 32 位无符号整数。
     * @param Output 接收字节的字符串。
     * @param Value 要追加的整数。
     */
    inline void AppendUint32(std::string &Output, std::uint32_t Value)
    {
        for (unsigned Shift = 0; Shift < 32; Shift += 8)
        {
            Output.push_back(static_cast<char>((Value >> Shift) & 0xffu));
        }
    }

    /**
     * @brief 从小端字节序缓冲区读取 32 位无符号整数。
     * @param Input 输入缓冲区。
     * @param Position 输入和输出位置；读取成功后前移四字节。
     * @param Value 接收读取结果。
     * @return 数据足够时读取成功并返回 `true`。
     */
    [[nodiscard]] inline auto ReadUint32(
        std::string_view Input, std::size_t &Position,
        std::uint32_t &Value) noexcept -> bool
    {
        if (Position > Input.size() || Input.size() - Position < 4)
        {
            return false;
        }
        Value = static_cast<std::uint32_t>(
            static_cast<unsigned char>(Input[Position]))
            | (static_cast<std::uint32_t>(
                static_cast<unsigned char>(Input[Position + 1])) << 8)
            | (static_cast<std::uint32_t>(
                static_cast<unsigned char>(Input[Position + 2])) << 16)
            | (static_cast<std::uint32_t>(
                static_cast<unsigned char>(Input[Position + 3])) << 24);
        Position += 4;
        return true;
    }

    // 包格式说明（简要）：
    // 二进制包 = "CW" + 1byte AlgId + salt(16) + 1byte IvLen + iv + 1byte TagLen/或 MacLen(当需要) + mac/tag + ciphertext

    /** @brief 加密数据包中使用的算法标识值。 */
    enum Binary : uint8_t
    {
        AlgAesGcm = 1, ///< AES-GCM。
        AlgAesCbc = 2, ///< AES-CBC 与 HMAC。
        AlgAesCtr = 3, ///< AES-CTR 与 HMAC。
        AlgTwofishCtr = 4 ///< Twofish-CTR 与 HMAC。
    };

    /**
     * @brief 提供 AES 和 Twofish 对称加解密接口。
     * @details CBC、CTR 和 Twofish-CTR 使用 HMAC-SHA256 校验完整性；GCM 使用内建认证标签。
     */
    class ArcaneSymmetric
    {
    public:
        /**
         * @brief 使用 AES-256-GCM 加密。
         * @param Plaintext 明文。
         * @param Pass 用于派生密钥的密码。
         * @return 包含 salt、IV、认证标签和密文的 Base64 字符串。
         */
        static std::string AesGcmEncrypt(const std::string &Plaintext, const std::string &Pass);

        /**
         * @brief 使用 AES-256-GCM 解密并验证认证标签。
         * @param CipherB64 `AesGcmEncrypt()` 返回的 Base64 字符串。
         * @param Pass 加密时使用的密码。
         * @return 解密后的明文。
         * @throws std::runtime_error 密文格式无效、解密失败或认证失败时抛出。
         */
        static std::string AesGcmDecrypt(const std::string &CipherB64, const std::string &Pass);

        /**
         * @brief 使用 AES-CBC 加密并附加 HMAC-SHA256。
         * @param Plaintext 明文。
         * @param Pass 用于派生密钥的密码。
         * @return 包含 salt、IV、MAC 和密文的 Base64 字符串。
         */
        static std::string AesCbcEncrypt(const std::string &Plaintext, const std::string &Pass);

        /**
         * @brief 使用 AES-CBC 解密并验证 HMAC-SHA256。
         * @param B64 `AesCbcEncrypt()` 返回的 Base64 字符串。
         * @param Pass 加密时使用的密码。
         * @return 解密后的明文。
         */
        static std::string AesCbcDecrypt(const std::string &B64, const std::string &Pass);

        /**
         * @brief 使用 AES-CTR 加密并附加 HMAC-SHA256。
         * @param Plaintext 明文。
         * @param Pass 用于派生密钥的密码。
         * @return 包含 salt、IV、MAC 和密文的 Base64 字符串。
         */
        static std::string AesCtrEncrypt(const std::string &Plaintext, const std::string &Pass);

        /**
         * @brief 使用 AES-CTR 解密并验证 HMAC-SHA256。
         * @param B64 `AesCtrEncrypt()` 返回的 Base64 字符串。
         * @param Pass 加密时使用的密码。
         * @return 解密后的明文。
         */
        static std::string AesCtrDecrypt(const std::string &B64, const std::string &Pass);

        /**
         * @brief 使用 Twofish-CTR 加密并附加 HMAC-SHA256。
         * @param Plaintext 明文。
         * @param Pass 用于派生密钥的密码。
         * @return 包含 salt、IV、MAC 和密文的 Base64 字符串。
         */
        static std::string TwofishCtrEncrypt(const std::string &Plaintext, const std::string &Pass);

        /**
         * @brief 使用 Twofish-CTR 解密并验证 HMAC-SHA256。
         * @param B64 `TwofishCtrEncrypt()` 返回的 Base64 字符串。
         * @param Pass 加密时使用的密码。
         * @return 解密后的明文。
         */
        static std::string TwofishCtrDecrypt(const std::string &B64, const std::string &Pass);
    };

    /**
     * @brief 提供 RSA 和 ElGamal 密钥生成、加解密及签名接口。
     * @details 密钥和密文以 Base64 编码的 DER 或数据文本传递。
     */
    class PenumbraAsymmetric
    {
    public:
        /**
         * @brief 生成 RSA 密钥对。
         * @param Bits 密钥长度，至少 2048 位。
         * @return 私钥和公钥的 Base64 DER 字符串。
         * @throws std::invalid_argument `Bits` 小于 2048 时抛出。
         */
        static std::pair<std::string, std::string> GenerateRSAKeypair(unsigned int Bits = 2048);

        /**
         * @brief 使用 RSA-OAEP 和公钥加密。
         * @param Plaintext 明文。
         * @param PubDerBase64 Base64 DER 格式的公钥。
         * @return Base64 编码的密文。
         */
        static std::string RsaOaepEncrypt(const std::string &Plaintext, const std::string &PubDerBase64);

        /**
         * @brief 使用 RSA-OAEP 和私钥解密。
         * @param CipherB64 Base64 编码的密文。
         * @param PrivDerBase64 Base64 DER 格式的私钥。
         * @return 解密后的明文。
         */
        static std::string RsaOaepDecrypt(const std::string &CipherB64, const std::string &PrivDerBase64);

        /**
         * @brief 使用 RSA-PSS 和 SHA-256 对消息签名。
         * @param Msg 待签名消息。
         * @param PrivDerBase64 Base64 DER 格式的私钥。
         * @return Base64 编码的签名。
         */
        static std::string RsaPssSign(const std::string &Msg, const std::string &PrivDerBase64);

        /**
         * @brief 使用 RSA-PSS 和 SHA-256 验证签名。
         * @param Msg 原始消息。
         * @param SigB64 Base64 编码的签名。
         * @param PubDerBase64 Base64 DER 格式的公钥。
         * @return 签名有效时为 `true`。
         */
        static bool RsaPssVerify(const std::string &Msg, const std::string &SigB64, const std::string &PubDerBase64);

        /**
         * @brief 生成 ElGamal 密钥对。
         * @param Pbits 参数位数，至少 2048 位。
         * @return 私钥和公钥的 Base64 DER 字符串。
         * @throws std::invalid_argument `Pbits` 小于 2048 时抛出。
         */
        static std::pair<std::string, std::string> GenerateElGamalKeypair(unsigned int Pbits = 2048);

        /**
         * @brief 使用 ElGamal 公钥加密。
         * @param Plaintext 明文。
         * @param PubDerBase64 Base64 DER 格式的公钥。
         * @return Base64 编码的密文。
         */
        static std::string ElGamalEncrypt(const std::string &Plaintext, const std::string &PubDerBase64);

        /**
         * @brief 使用 ElGamal 私钥解密。
         * @param CipherB64 Base64 编码的密文。
         * @param PrivDerBase64 Base64 DER 格式的私钥。
         * @return 解密后的明文。
         */
        static std::string ElGamalDecrypt(const std::string &CipherB64, const std::string &PrivDerBase64);
    };

    // ---------- 哈希辅助 ----------
    /**
     * @brief 提供 MD5 和 SHA-256 摘要函数。
     * @note MD5 仅用于兼容性或非安全校验，不应用于安全场景。
     */
    class UmbrageHash
    {
    public:
        /**
         * @brief 计算 MD5 摘要并返回十六进制文本。
         * @param Data 输入数据。
         * @return 32 个十六进制字符。
         */
        inline static std::string Md5(const std::string &Data);

        /**
         * @brief 计算 SHA-256 摘要并返回十六进制文本。
         * @param Data 输入数据。
         * @return 64 个十六进制字符。
         */
        inline static std::string Sha256(const std::string &Data);
    };

    // 实现: ArcaneSymmetric
    std::string ArcaneSymmetric::AesGcmEncrypt(const std::string &Plaintext, const std::string &Pass)
    {
        AeonRandom Rng;
        const size_t SaltLen = 16;
        const size_t IvLen = 12;
        const size_t KeyLen = 32; // AES-256-GCM

        CryptoPP::SecByteBlock Salt(SaltLen);
        Rng.Pool().GenerateBlock(Salt, SaltLen);
        std::string Key = DeriveKeyFromPassword(Pass, Salt.begin(), SaltLen, KeyLen);

        CryptoPP::SecByteBlock Iv(IvLen);
        Rng.Pool().GenerateBlock(Iv, IvLen);

        std::string CipherAndTag;
        try
        {
            CryptoPP::GCM<CryptoPP::AES>::Encryption Enc;
            Enc.SetKeyWithIV((const CryptoByte *)Key.data(), Key.size(), Iv, Iv.size());

            CryptoPP::StringSource Ss(Plaintext, true, new CryptoPP::AuthenticatedEncryptionFilter(Enc, new CryptoPP::StringSink(CipherAndTag), false, 16));
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("AESGCM encrypt error: ") + E.what());
        }

        std::string Bin;
        Bin.reserve(2 + 1 + SaltLen + 1 + IvLen + 1 + 16 + CipherAndTag.size());
        Bin.append("CW", 2);
        Bin.push_back((char)AlgAesGcm);
        Bin.append((const char *)Salt.BytePtr(), SaltLen);
        Bin.push_back((char)IvLen);
        Bin.append((const char *)Iv.BytePtr(), IvLen);
        Bin.push_back((char)16);
        Bin.append(CipherAndTag);
        return ToBase64(Bin);
    }

    std::string ArcaneSymmetric::AesGcmDecrypt(const std::string &CipherB64, const std::string &Pass)
    {
        std::string Bin = FromBase64(CipherB64);
        if (Bin.size() < 2 + 1 + 16 + 1 + 12 + 1 + 16)
            throw std::runtime_error("Invalid ciphertext (too short)");
        size_t Pos = 0;
        if (Bin[0] != 'C' || Bin[1] != 'W')
            throw std::runtime_error("Invalid format");
        Pos = 2;
        uint8_t Alg = (uint8_t)Bin[Pos++];
        if (Alg != AlgAesGcm)
            throw std::runtime_error("Algorithm mismatch (expect AES-GCM)");
        const size_t SaltLen = 16;
        const size_t SaltPos = Pos;
        Pos += SaltLen;
        if (Bin.size() < Pos + 1)
            throw std::runtime_error("Invalid ciphertext (missing IV length)");
        uint8_t IvLen = (uint8_t)Bin[Pos++];
        if (IvLen != 12 || Bin.size() < Pos + IvLen + 1)
            throw std::runtime_error("Invalid ciphertext (invalid IV)");
        const size_t IvPos = Pos;
        Pos += IvLen;
        uint8_t TagLen = (uint8_t)Bin[Pos++];
        if (TagLen != 16 || Bin.size() < Pos + TagLen)
            throw std::runtime_error("Invalid ciphertext (invalid tag)");
        std::string CipherAndTag = Bin.substr(Pos);

        std::string Key = DeriveKeyFromPassword(Pass, (const CryptoByte *)Bin.data() + SaltPos, SaltLen, 32);

        try
        {
            CryptoPP::GCM<CryptoPP::AES>::Decryption Dec;
            Dec.SetKeyWithIV((const CryptoByte *)Key.data(), Key.size(), (const CryptoByte *)(Bin.data() + IvPos), IvLen);

            std::string Recovered;
            CryptoPP::AuthenticatedDecryptionFilter Df(Dec, new CryptoPP::StringSink(Recovered),
                                                       CryptoPP::AuthenticatedDecryptionFilter::THROW_EXCEPTION, TagLen);
            CryptoPP::StringSource Ss(CipherAndTag, true, new CryptoPP::Redirector(Df));
            return Recovered;
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("AESGCM decrypt error: ") + E.what());
        }
    }

    std::string ArcaneSymmetric::AesCbcEncrypt(const std::string &Plaintext, const std::string &Pass)
    {
        AeonRandom Rng;
        const size_t SaltLen = 16;
        const size_t IvLen = 16;
        const size_t KeyLen = 32; // AES-256
        CryptoPP::SecByteBlock Salt(SaltLen);
        Rng.Pool().GenerateBlock(Salt, SaltLen);
        std::string Key = DeriveKeyFromPassword(Pass, Salt.BytePtr(), SaltLen, KeyLen);
        CryptoPP::SecByteBlock Iv(IvLen);
        Rng.Pool().GenerateBlock(Iv, IvLen);

        std::string Ciphertext;
        try
        {
            CryptoPP::CBC_Mode<CryptoPP::AES>::Encryption Enc((const CryptoByte *)Key.data(), Key.size(), Iv);
            CryptoPP::StringSource Ss(Plaintext, true, new CryptoPP::StreamTransformationFilter(Enc, new CryptoPP::StringSink(Ciphertext), CryptoPP::BlockPaddingSchemeDef::PKCS_PADDING));
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("AESCBC encrypt error: ") + E.what());
        }

        std::string MacKey = DeriveKeyFromPassword(Pass, Salt.BytePtr(), SaltLen, 32, 200000);
        std::string Mac;
        try
        {
            CryptoPP::HMAC<CryptoPP::SHA256> Hmac((const CryptoByte *)MacKey.data(), MacKey.size());
            CryptoPP::StringSource Ss(std::string((const char *)Iv.BytePtr(), Iv.size()) + Ciphertext, true,
                                      new CryptoPP::HashFilter(Hmac, new CryptoPP::StringSink(Mac)));
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("AESCBC HMAC error: ") + E.what());
        }

        std::string Bin;
        Bin.append("CW", 2);
        Bin.push_back((char)AlgAesCbc);
        Bin.append((const char *)Salt.BytePtr(), SaltLen);
        Bin.push_back((char)IvLen);
        Bin.append((const char *)Iv.BytePtr(), IvLen);
        uint32_t MacLen = (uint32_t)Mac.size();
        AppendUint32(Bin, MacLen);
        Bin.append(Mac);
        Bin.append(Ciphertext);
        return ToBase64(Bin);
    }

    std::string ArcaneSymmetric::AesCbcDecrypt(const std::string &B64, const std::string &Pass)
    {
        std::string Bin = FromBase64(B64);
        if (Bin.size() < 2 + 1 + 16 + 1 + 16 + 4)
            throw std::runtime_error("Invalid ciphertext (AESCBC too short)");
        if (Bin[0] != 'C' || Bin[1] != 'W')
            throw std::runtime_error("Invalid format");
        size_t Pos = 2;
        uint8_t Alg = (uint8_t)Bin[Pos++];
        if (Alg != AlgAesCbc)
            throw std::runtime_error("Algorithm mismatch (expect AES-CBC)");
        const size_t SaltLen = 16;
        const size_t SaltPos = Pos;
        Pos += SaltLen;
        if (Bin.size() < Pos + 1)
            throw std::runtime_error("Invalid ciphertext (missing IV length)");
        uint8_t IvLen = (uint8_t)Bin[Pos++];
        if (IvLen != 16 || Bin.size() < Pos + IvLen + sizeof(std::uint32_t))
            throw std::runtime_error("Invalid ciphertext (invalid IV)");
        const size_t IvPos = Pos;
        Pos += IvLen;
        uint32_t MacLen = 0;
        if (!ReadUint32(Bin, Pos, MacLen))
            throw std::runtime_error("Invalid ciphertext (mac length missing)");
        if (MacLen != 32 || MacLen > Bin.size() - Pos)
            throw std::runtime_error("Invalid ciphertext (mac missing)");
        std::string Mac = Bin.substr(Pos, MacLen);
        Pos += MacLen;
        std::string Ciphertext = Bin.substr(Pos);

        std::string MacKey = DeriveKeyFromPassword(Pass, (const CryptoByte *)Bin.data() + SaltPos, SaltLen, 32, 200000);
        std::string MacCheck;
        try
        {
            CryptoPP::HMAC<CryptoPP::SHA256> Hmac((const CryptoByte *)MacKey.data(), MacKey.size());
            CryptoPP::StringSource Ss(std::string((const char *)(Bin.data() + IvPos), IvLen) + Ciphertext, true,
                                      new CryptoPP::HashFilter(Hmac, new CryptoPP::StringSink(MacCheck)));
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("AESCBC HMAC verify error: ") + E.what());
        }
        if (!ConstantTimeEqual(Mac, MacCheck))
            throw std::runtime_error("AESCBC HMAC mismatch - integrity failure");

        std::string Key = DeriveKeyFromPassword(Pass, (const CryptoByte *)Bin.data() + SaltPos, SaltLen, 32);
        try
        {
            CryptoPP::CBC_Mode<CryptoPP::AES>::Decryption Dec((const CryptoByte *)Key.data(), Key.size(), (const CryptoByte *)(Bin.data() + IvPos));
            std::string Recovered;
            CryptoPP::StringSource Ss(Ciphertext, true, new CryptoPP::StreamTransformationFilter(Dec, new CryptoPP::StringSink(Recovered), CryptoPP::BlockPaddingSchemeDef::PKCS_PADDING));
            return Recovered;
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("AESCBC decrypt error: ") + E.what());
        }
    }

    std::string ArcaneSymmetric::AesCtrEncrypt(const std::string &Plaintext, const std::string &Pass)
    {
        AeonRandom Rng;
        const size_t SaltLen = 16;
        const size_t IvLen = 16;
        const size_t KeyLen = 32;
        CryptoPP::SecByteBlock Salt(SaltLen);
        Rng.Pool().GenerateBlock(Salt, SaltLen);
        std::string Key = DeriveKeyFromPassword(Pass, Salt.BytePtr(), SaltLen, KeyLen);
        CryptoPP::SecByteBlock Iv(IvLen);
        Rng.Pool().GenerateBlock(Iv, IvLen);

        std::string Ciphertext;
        try
        {
            CryptoPP::CTR_Mode<CryptoPP::AES>::Encryption Enc((const CryptoByte *)Key.data(), Key.size(), Iv);
            CryptoPP::StringSource Ss(Plaintext, true, new CryptoPP::StreamTransformationFilter(Enc, new CryptoPP::StringSink(Ciphertext)));
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("AESCTR encrypt error: ") + E.what());
        }

        std::string MacKey = DeriveKeyFromPassword(Pass, Salt.BytePtr(), SaltLen, 32, 200000);
        std::string Mac;
        CryptoPP::HMAC<CryptoPP::SHA256> Hmac((const CryptoByte *)MacKey.data(), MacKey.size());
        CryptoPP::StringSource Ss(std::string((const char *)Iv.BytePtr(), Iv.size()) + Ciphertext, true,
                                  new CryptoPP::HashFilter(Hmac, new CryptoPP::StringSink(Mac)));

        std::string Bin;
        Bin.append("CW", 2);
        Bin.push_back((char)AlgAesCtr);
        Bin.append((const char *)Salt.BytePtr(), SaltLen);
        Bin.push_back((char)IvLen);
        Bin.append((const char *)Iv.BytePtr(), IvLen);
        uint32_t MacLen = (uint32_t)Mac.size();
        AppendUint32(Bin, MacLen);
        Bin.append(Mac);
        Bin.append(Ciphertext);
        return ToBase64(Bin);
    }

    std::string ArcaneSymmetric::AesCtrDecrypt(const std::string &B64, const std::string &Pass)
    {
        std::string Bin = FromBase64(B64);
        if (Bin.size() < 2 + 1 + 16 + 1 + 16 + sizeof(std::uint32_t))
            throw std::runtime_error("Invalid ciphertext (AESCTR too short)");
        size_t Pos = 2;
        if (Bin[0] != 'C' || Bin[1] != 'W')
            throw std::runtime_error("Invalid format");
        uint8_t Alg = (uint8_t)Bin[Pos++];
        if (Alg != AlgAesCtr)
            throw std::runtime_error("Algorithm mismatch (expect AES-CTR)");
        const size_t SaltLen = 16;
        size_t SaltPos = Pos;
        Pos += SaltLen;
        if (Bin.size() < Pos + 1)
            throw std::runtime_error("Invalid ciphertext (missing IV length)");
        uint8_t IvLen = (uint8_t)Bin[Pos++];
        if (IvLen != 16 || Bin.size() < Pos + IvLen + sizeof(std::uint32_t))
            throw std::runtime_error("Invalid ciphertext (invalid IV)");
        size_t IvPos = Pos;
        Pos += IvLen;
        uint32_t MacLen = 0;
        if (!ReadUint32(Bin, Pos, MacLen))
            throw std::runtime_error("Invalid ciphertext (MAC length missing)");
        if (MacLen != 32 || MacLen > Bin.size() - Pos)
            throw std::runtime_error("Invalid ciphertext (invalid MAC)");
        std::string Mac = Bin.substr(Pos, MacLen);
        Pos += MacLen;
        std::string Ciphertext = Bin.substr(Pos);

        std::string MacKey = DeriveKeyFromPassword(Pass, (const CryptoByte *)Bin.data() + SaltPos, SaltLen, 32, 200000);
        std::string MacCheck;
        CryptoPP::HMAC<CryptoPP::SHA256> Hmac((const CryptoByte *)MacKey.data(), MacKey.size());
        CryptoPP::StringSource Ss(std::string((const char *)(Bin.data() + IvPos), IvLen) + Ciphertext, true,
                                  new CryptoPP::HashFilter(Hmac, new CryptoPP::StringSink(MacCheck)));
        if (!ConstantTimeEqual(Mac, MacCheck))
            throw std::runtime_error("AESCTR HMAC mismatch - integrity failure");

        std::string Key = DeriveKeyFromPassword(Pass, (const CryptoByte *)Bin.data() + SaltPos, SaltLen, 32);
        try
        {
            std::string Recovered;
            CryptoPP::CTR_Mode<CryptoPP::AES>::Decryption Dec((const CryptoByte *)Key.data(), Key.size(), (const CryptoByte *)(Bin.data() + IvPos));
            CryptoPP::StringSource Ss2(Ciphertext, true, new CryptoPP::StreamTransformationFilter(Dec, new CryptoPP::StringSink(Recovered)));
            return Recovered;
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("AESCTR decrypt error: ") + E.what());
        }
    }

    std::string ArcaneSymmetric::TwofishCtrEncrypt(const std::string &Plaintext, const std::string &Pass)
    {
        AeonRandom Rng;
        const size_t SaltLen = 16;
        const size_t IvLen = 16;
        const size_t KeyLen = 32;
        CryptoPP::SecByteBlock Salt(SaltLen);
        Rng.Pool().GenerateBlock(Salt, SaltLen);
        std::string Key = DeriveKeyFromPassword(Pass, Salt.BytePtr(), SaltLen, KeyLen);
        CryptoPP::SecByteBlock Iv(IvLen);
        Rng.Pool().GenerateBlock(Iv, IvLen);

        std::string Ciphertext;
        try
        {
            CryptoPP::CTR_Mode<CryptoPP::Twofish>::Encryption Enc((const CryptoByte *)Key.data(), Key.size(), Iv);
            CryptoPP::StringSource Ss(Plaintext, true, new CryptoPP::StreamTransformationFilter(Enc, new CryptoPP::StringSink(Ciphertext)));
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("TwofishCTR encrypt error: ") + E.what());
        }

        std::string MacKey = DeriveKeyFromPassword(Pass, Salt.BytePtr(), SaltLen, 32, 200000);
        std::string Mac;
        CryptoPP::HMAC<CryptoPP::SHA256> Hmac((const CryptoByte *)MacKey.data(), MacKey.size());
        CryptoPP::StringSource Ss(std::string((const char *)Iv.BytePtr(), Iv.size()) + Ciphertext, true,
                                  new CryptoPP::HashFilter(Hmac, new CryptoPP::StringSink(Mac)));

        std::string Bin;
        Bin.append("CW", 2);
        Bin.push_back((char)AlgTwofishCtr);
        Bin.append((const char *)Salt.BytePtr(), SaltLen);
        Bin.push_back((char)IvLen);
        Bin.append((const char *)Iv.BytePtr(), IvLen);
        uint32_t MacLen = (uint32_t)Mac.size();
        AppendUint32(Bin, MacLen);
        Bin.append(Mac);
        Bin.append(Ciphertext);
        return ToBase64(Bin);
    }

    std::string ArcaneSymmetric::TwofishCtrDecrypt(const std::string &B64, const std::string &Pass)
    {
        std::string Bin = FromBase64(B64);
        if (Bin.size() < 2 + 1 + 16 + 1 + 16 + 4)
            throw std::runtime_error("Invalid Twofish ciphertext");
        if (Bin[0] != 'C' || Bin[1] != 'W')
            throw std::runtime_error("Invalid format");
        size_t Pos = 2;
        uint8_t Alg = (uint8_t)Bin[Pos++];
        if (Alg != AlgTwofishCtr)
            throw std::runtime_error("Algorithm mismatch (expect Twofish-CTR)");
        const size_t SaltLen = 16;
        size_t SaltPos = Pos;
        Pos += SaltLen;
        if (Bin.size() < Pos + 1)
            throw std::runtime_error("Invalid ciphertext (missing IV length)");
        uint8_t IvLen = (uint8_t)Bin[Pos++];
        if (IvLen != 16 || Bin.size() < Pos + IvLen + sizeof(std::uint32_t))
            throw std::runtime_error("Invalid ciphertext (invalid IV)");
        size_t IvPos = Pos;
        Pos += IvLen;
        uint32_t MacLen = 0;
        if (!ReadUint32(Bin, Pos, MacLen))
            throw std::runtime_error("Invalid ciphertext (MAC length missing)");
        if (MacLen != 32 || MacLen > Bin.size() - Pos)
            throw std::runtime_error("Invalid ciphertext (invalid MAC)");
        std::string Mac = Bin.substr(Pos, MacLen);
        Pos += MacLen;
        std::string Ciphertext = Bin.substr(Pos);

        std::string MacKey = DeriveKeyFromPassword(Pass, (const CryptoByte *)Bin.data() + SaltPos, SaltLen, 32, 200000);
        std::string MacCheck;
        CryptoPP::HMAC<CryptoPP::SHA256> Hmac((const CryptoByte *)MacKey.data(), MacKey.size());
        CryptoPP::StringSource Ss(std::string((const char *)(Bin.data() + IvPos), IvLen) + Ciphertext, true,
                                  new CryptoPP::HashFilter(Hmac, new CryptoPP::StringSink(MacCheck)));
        if (!ConstantTimeEqual(Mac, MacCheck))
            throw std::runtime_error("Twofish HMAC mismatch");

        std::string Key = DeriveKeyFromPassword(Pass, (const CryptoByte *)Bin.data() + SaltPos, SaltLen, 32);
        try
        {
            std::string Recovered;
            CryptoPP::CTR_Mode<CryptoPP::Twofish>::Decryption Dec((const CryptoByte *)Key.data(), Key.size(), (const CryptoByte *)(Bin.data() + IvPos));
            CryptoPP::StringSource Ss2(Ciphertext, true, new CryptoPP::StreamTransformationFilter(Dec, new CryptoPP::StringSink(Recovered)));
            return Recovered;
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("TwofishCTR decrypt error: ") + E.what());
        }
    }

    // 实现: PenumbraAsymmetric
    std::pair<std::string, std::string> PenumbraAsymmetric::GenerateRSAKeypair(unsigned int Bits)
    {
        if (Bits < 2048)
        {
            throw std::invalid_argument("RSA key size must be at least 2048 bits");
        }
        AeonRandom Rng;
        CryptoPP::AutoSeededRandomPool &Prng = Rng.Pool();
        CryptoPP::InvertibleRSAFunction Params;
        Params.Initialize(Prng, Bits);

        CryptoPP::RSA::PrivateKey Priv(Params);
        CryptoPP::RSA::PublicKey Pub(Params);

        std::string PrivDer, PubDer;
        CryptoPP::ByteQueue Qb;
        Priv.Save(Qb);
        CryptoPP::StringSink Ss1(PrivDer);
        Qb.CopyTo(Ss1);
        Ss1.MessageEnd();

        CryptoPP::ByteQueue Qb2;
        Pub.Save(Qb2);
        CryptoPP::StringSink Ss2(PubDer);
        Qb2.CopyTo(Ss2);
        Ss2.MessageEnd();

        return {ToBase64(PrivDer), ToBase64(PubDer)};
    }

    std::string PenumbraAsymmetric::RsaOaepEncrypt(const std::string &Plaintext, const std::string &PubDerBase64)
    {
        std::string PubDer = FromBase64(PubDerBase64);
        CryptoPP::ByteQueue Queue;
        Queue.Put((const CryptoByte *)PubDer.data(), PubDer.size());
        Queue.MessageEnd();
        CryptoPP::RSA::PublicKey Pub;
        Pub.Load(Queue);

        AeonRandom Rng;
        CryptoPP::AutoSeededRandomPool &Prng = Rng.Pool();
        std::string Cipher;
        try
        {
            CryptoPP::RSAES_OAEP_SHA_Encryptor Enc(Pub);
            CryptoPP::StringSource Ss(Plaintext, true, new CryptoPP::PK_EncryptorFilter(Prng, Enc, new CryptoPP::StringSink(Cipher)));
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("RSA OAEP encrypt error: ") + E.what());
        }
        return ToBase64(Cipher);
    }

    std::string PenumbraAsymmetric::RsaOaepDecrypt(const std::string &CipherB64, const std::string &PrivDerBase64)
    {
        std::string PrivDer = FromBase64(PrivDerBase64);
        CryptoPP::ByteQueue Queue;
        Queue.Put((const CryptoByte *)PrivDer.data(), PrivDer.size());
        Queue.MessageEnd();
        CryptoPP::RSA::PrivateKey Priv;
        Priv.Load(Queue);

        std::string Cipher = FromBase64(CipherB64);
        AeonRandom Rng;
        CryptoPP::AutoSeededRandomPool &Prng = Rng.Pool();
        try
        {
            CryptoPP::RSAES_OAEP_SHA_Decryptor Dec(Priv);
            std::string Recovered;
            CryptoPP::StringSource Ss(Cipher, true, new CryptoPP::PK_DecryptorFilter(Prng, Dec, new CryptoPP::StringSink(Recovered)));
            return Recovered;
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("RSA OAEP decrypt error: ") + E.what());
        }
    }

    std::string PenumbraAsymmetric::RsaPssSign(const std::string &Msg, const std::string &PrivDerBase64)
    {
        std::string PrivDer = FromBase64(PrivDerBase64);
        CryptoPP::ByteQueue Qb;
        Qb.Put((const CryptoByte *)PrivDer.data(), PrivDer.size());
        Qb.MessageEnd();
        CryptoPP::RSA::PrivateKey Priv;
        Priv.Load(Qb);

        AeonRandom Rng;
        CryptoPP::AutoSeededRandomPool &Prng = Rng.Pool();
        std::string Signature;
        try
        {
            CryptoPP::RSASS<CryptoPP::PSS, CryptoPP::SHA256>::Signer Signer(Priv);
            CryptoPP::StringSource Ss(Msg, true, new CryptoPP::SignerFilter(Prng, Signer, new CryptoPP::StringSink(Signature)));
            return ToBase64(Signature);
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("RSA PSS sign error: ") + E.what());
        }
    }

    bool PenumbraAsymmetric::RsaPssVerify(const std::string &Msg, const std::string &SigB64, const std::string &PubDerBase64)
    {
        try
        {
            std::string PubDer = FromBase64(PubDerBase64);
            CryptoPP::ByteQueue Qb;
            Qb.Put((const CryptoByte *)PubDer.data(), PubDer.size());
            Qb.MessageEnd();
            CryptoPP::RSA::PublicKey Pub;
            Pub.Load(Qb);

            std::string Signature = FromBase64(SigB64);
            CryptoPP::RSASS<CryptoPP::PSS, CryptoPP::SHA256>::Verifier Verifier(Pub);
            bool Ok = Verifier.VerifyMessage((const CryptoByte *)Msg.data(), Msg.size(), (const CryptoByte *)Signature.data(), Signature.size());
            return Ok;
        }
        catch (...)
        {
            return false;
        }
    }

    std::pair<std::string, std::string> PenumbraAsymmetric::GenerateElGamalKeypair(unsigned int Pbits)
    {
        if (Pbits < 2048)
        {
            throw std::invalid_argument(
                "ElGamal group size must be at least 2048 bits");
        }
        AeonRandom Rng;
        CryptoPP::AutoSeededRandomPool &Prng = Rng.Pool();

        CryptoPP::DL_GroupParameters_GFP Params;
        Params.GenerateRandomWithKeySize(Prng, Pbits);
        CryptoPP::ElGamalKeys::PrivateKey Priv;
        CryptoPP::ElGamalKeys::PublicKey Pub;
        Priv.GenerateRandom(Prng, Params);
        Priv.MakePublicKey(Pub);

        std::string PrivDer, PubDer;
        CryptoPP::ByteQueue Qb;
        Priv.Save(Qb);
        Qb.MessageEnd();
        CryptoPP::StringSink Ss1(PrivDer);
        Qb.CopyTo(Ss1);
        Ss1.MessageEnd();
        CryptoPP::ByteQueue Qb2;
        Pub.Save(Qb2);
        Qb2.MessageEnd();
        CryptoPP::StringSink Ss2(PubDer);
        Qb2.CopyTo(Ss2);
        Ss2.MessageEnd();
        return {ToBase64(PrivDer), ToBase64(PubDer)};
    }

    std::string PenumbraAsymmetric::ElGamalEncrypt(const std::string &Plaintext, const std::string &PubDerBase64)
    {
        std::string PubDer = FromBase64(PubDerBase64);
        CryptoPP::ByteQueue Qb;
        Qb.Put((const CryptoByte *)PubDer.data(), PubDer.size());
        Qb.MessageEnd();
        CryptoPP::ElGamalKeys::PublicKey Pub;
        Pub.Load(Qb);
        AeonRandom Rng;
        CryptoPP::AutoSeededRandomPool &Prng = Rng.Pool();
        std::string Cipher;
        try
        {
            CryptoPP::ElGamalEncryptor Enc(Pub);
            CryptoPP::StringSource Ss(Plaintext, true, new CryptoPP::PK_EncryptorFilter(Prng, Enc, new CryptoPP::StringSink(Cipher)));
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("ElGamal encrypt error: ") + E.what());
        }
        return ToBase64(Cipher);
    }

    std::string PenumbraAsymmetric::ElGamalDecrypt(const std::string &CipherB64, const std::string &PrivDerBase64)
    {
        std::string PrivDer = FromBase64(PrivDerBase64);
        CryptoPP::ByteQueue Qb;
        Qb.Put((const CryptoByte *)PrivDer.data(), PrivDer.size());
        Qb.MessageEnd();
        CryptoPP::ElGamalKeys::PrivateKey Priv;
        Priv.Load(Qb);
        std::string Cipher = FromBase64(CipherB64);
        AeonRandom Rng;
        CryptoPP::AutoSeededRandomPool &Prng = Rng.Pool();
        try
        {
            CryptoPP::ElGamalDecryptor Dec(Priv);
            std::string Recovered;
            CryptoPP::StringSource Ss(Cipher, true, new CryptoPP::PK_DecryptorFilter(Prng, Dec, new CryptoPP::StringSink(Recovered)));
            return Recovered;
        }
        catch (const CryptoPP::Exception &E)
        {
            throw std::runtime_error(std::string("ElGamal decrypt error: ") + E.what());
        }
    }

    inline std::string UmbrageHash::Md5(const std::string &Data)
    {
        std::string Out;
        CryptoPP::Weak::MD5 Md;
        CryptoPP::StringSource Ss(Data, true, new CryptoPP::HashFilter(Md, new CryptoPP::HexEncoder(new CryptoPP::StringSink(Out), false)));
        return Out;
    }

    inline std::string UmbrageHash::Sha256(const std::string &Data)
    {
        std::string Out;
        CryptoPP::SHA256 Sha;
        CryptoPP::StringSource Ss(Data, true, new CryptoPP::HashFilter(Sha, new CryptoPP::HexEncoder(new CryptoPP::StringSink(Out), false)));
        return Out;
    }

    /**
     * @brief 计算数据的 CRC-32 校验值。
     * @param Data 要校验的字节串。
     * @param Length 要处理的字节数；默认值表示处理整个字符串。
     * @return CRC-32 校验值。
     * @throws std::out_of_range Length 大于字符串长度时抛出。
     * @note CRC-32 只用于错误检测，不能验证数据来源或抵抗恶意篡改。
     */
    inline std::uint32_t Crc32(const std::string &Data,
        const std::uint64_t Length = std::numeric_limits<std::uint64_t>::max())
    {
        static constexpr std::uint32_t MappingTable[256] =
            {
                0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
                0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
                0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
                0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
                0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
                0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
                0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
                0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
                0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
                0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
                0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
                0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
                0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
                0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
                0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
                0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
                0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
                0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
                0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
                0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
                0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
                0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
                0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
                0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
                0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
                0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
                0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
                0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
                0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
                0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
                0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
                0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};
        const auto EffectiveLength = Length == std::numeric_limits<std::uint64_t>::max()
            ? static_cast<std::uint64_t>(Data.size())
            : Length;
        if (EffectiveLength > Data.size())
        {
            throw std::out_of_range("CRC32 length exceeds input size");
        }

        const uint8_t *Bytes = reinterpret_cast<const uint8_t *>(Data.data());
        uint32_t CrcValue = 0xFFFFFFFF;

        for (size_t I = 0; I < EffectiveLength; ++I)
        {
            CrcValue = (CrcValue >> 8) ^ MappingTable[(CrcValue ^ Bytes[I]) & 0xFF];
        }

        return static_cast<std::uint32_t>(~CrcValue);
    }

    /**
     * @brief 生成由时间、计数器和常量混合而成的 64 字节数据。
     * @return 含 64 字节混合数据的字符串。
     * @note 该函数不提供密码学安全随机数，不应用于令牌或密钥生成。
     */
    inline std::string Mix64()
    {
        alignas(64) char Buf[64];

        static constexpr char KKey[32] =
            "\x87\x31\xa2\x4b\xfa\x8f\x63\x27"
            "\xad\x2e\xeb\x19\x1c\x6b\x93\x0f"
            "\x4d\x5a\x76\x42\x13\x37\x55\xde"
            "\xad\xbe\xef\x8a\x24\xbc\xda";
        std::memcpy(Buf, KKey, 32);

        uint64_t Ts = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        std::memcpy(Buf + 32, &Ts, 8);

#if defined(__GNUC__) || defined(__clang__)
        uint64_t Tsc = __builtin_ia32_rdtsc();
#elif defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
        uint64_t Tsc = __rdtsc();
#else
        uint64_t Tsc = static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
#endif
        std::memcpy(Buf + 40, &Tsc, 8);

        constexpr std::uint64_t MarkerA = 0xDEADBEEFC0FFEEUL;
        constexpr std::uint64_t MarkerB = 0x1234567890ABCDEFUL;
        std::memcpy(Buf + 48, &MarkerA, sizeof(MarkerA));
        std::memcpy(Buf + 56, &MarkerB, sizeof(MarkerB));

        return std::string(Buf, 64);
    }
} // end Encryption
