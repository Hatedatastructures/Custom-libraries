/**
 * @file Session.hpp
 * @brief 基于 token 的线程安全 Session 存储。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <mutex>
#include <optional>
#include <osrng.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace Wan::Web
{
    /**
     * @struct SessionData
     * @brief 一份 Session 数据快照。
     */
    struct SessionData
    {
        /** @brief Session 标识。 */
        std::string Id;
        /** @brief Session 键值。 */
        std::unordered_map<std::string, std::string> Values;
        /** @brief 基于 steady clock 的过期时间点。 */
        std::chrono::steady_clock::time_point Expires;
    };

    /**
     * @struct SessionConfig
     * @brief Session 存储资源上限。
     */
    struct SessionConfig
    {
        /** @brief 可同时保存的 Session 数量上限。 */
        std::size_t MaxSessions = 10'000;
        /** @brief 每个 Session 可保存的键值数量上限。 */
        std::size_t MaxValuesPerSession = 64;
        /** @brief 键的最大字节数。 */
        std::size_t MaxKeySize = 256;
        /** @brief 值的最大字节数。 */
        std::size_t MaxValueSize = 64 * 1024;
    };

    /**
     * @class SessionStore
     * @brief 线程安全的内存 Session 存储。
     */
    class SessionStore
    {
    public:
        /**
         * @brief 创建 Session 存储。
         * @param Config 资源上限配置。
         * @throws std::invalid_argument 任一资源上限为零时抛出。
         */
        explicit SessionStore(SessionConfig Config = {})
            : MemberConfig(std::move(Config))
        {
            ValidateConfiguration();
        }

        /**
         * @brief 创建新 Session。
         * @param Ttl 有效时长，必须大于零且可由 steady clock 表示。
         * @return 新 Session 的数据副本。
         * @throws std::invalid_argument `Ttl` 无效或不可表示时抛出。
         * @throws std::length_error 存储容量已满时抛出。
         * @throws std::runtime_error 多次生成标识仍发生冲突时抛出。
         */
        [[nodiscard]] auto Create(
            std::chrono::seconds Ttl = std::chrono::seconds{3600})
            -> SessionData
        {
            auto Now = std::chrono::steady_clock::now();
            if (Ttl <= std::chrono::seconds::zero()
                || !IsTtlRepresentable(Ttl, Now))
            {
                throw std::invalid_argument(
                    "session lifetime must be positive and representable");
            }
            std::lock_guard<std::mutex> Lock{MemberMutex};
            Now = std::chrono::steady_clock::now();
            if (!IsTtlRepresentable(Ttl, Now))
            {
                throw std::invalid_argument(
                    "session lifetime is outside the steady clock range");
            }
            CleanupExpiredLocked(Now);
            if (MemberSessions.size() >= MemberConfig.MaxSessions)
            {
                throw std::length_error("session store capacity exhausted");
            }

            SessionData S;
            bool Unique = false;
            for (int Attempt = 0; Attempt < 16; ++Attempt)
            {
                S.Id = GenerateId();
                if (MemberSessions.find(S.Id) == MemberSessions.end())
                {
                    Unique = true;
                    break;
                }
            }
            if (!Unique)
            {
                throw std::runtime_error(
                    "unable to allocate a unique session id");
            }
            S.Expires = Now + Ttl;
            MemberSessions[S.Id] = S;
            return S;
        }

        /**
         * @brief 获取 Session 数据快照。
         * @param Id Session 标识。
         * @return Session 存在且未过期时返回副本，否则返回空值。
         */
        [[nodiscard]] auto Get(const std::string& Id)
            -> std::optional<SessionData>
        {
            std::lock_guard<std::mutex> Lock{MemberMutex};

            auto It = MemberSessions.find(Id);
            if (It == MemberSessions.end())
            {
                return std::nullopt;
            }

            if (std::chrono::steady_clock::now() >= It->second.Expires)
            {
                MemberSessions.erase(It);
                return std::nullopt;
            }

            return It->second;
        }

        /**
         * @brief 设置 Session 键值。
         * @param Id Session 标识。
         * @param Key 键。
         * @param Value 值。
         * @throws std::length_error 键、值或该 Session 的键值数量超出配置时抛出。
         * @note Session 不存在或已过期时不执行更新。
         */
        // The public API intentionally keeps the conventional Id/Key/Value order.
        // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
        void Set(const std::string& Id,
            const std::string& Key, const std::string& Value)
        {
            std::lock_guard<std::mutex> Lock{MemberMutex};

            auto It = MemberSessions.find(Id);
            if (It == MemberSessions.end())
            {
                return;
            }
            if (std::chrono::steady_clock::now() >= It->second.Expires)
            {
                MemberSessions.erase(It);
                return;
            }
            if (Key.size() > MemberConfig.MaxKeySize
                || Value.size() > MemberConfig.MaxValueSize)
            {
                throw std::length_error(
                    "session key or value exceeds configured limits");
            }
            if (It->second.Values.find(Key) == It->second.Values.end()
                && It->second.Values.size()
                    >= MemberConfig.MaxValuesPerSession)
            {
                throw std::length_error(
                    "session value count exceeds configured limit");
            }
            It->second.Values[Key] = Value;
        }

        /**
         * @brief 删除 Session。
         * @param Id 要删除的 Session 标识。
         */
        void Destroy(const std::string& Id)
        {
            std::lock_guard<std::mutex> Lock{MemberMutex};
            MemberSessions.erase(Id);
        }

        /**
         * @brief 清理所有已过期的 Session。
         */
        void Cleanup()
        {
            std::lock_guard<std::mutex> Lock{MemberMutex};
            CleanupExpiredLocked(std::chrono::steady_clock::now());
        }

    private:
        /**
         * @brief 检查 TTL 是否为正数且不会超出 steady clock 范围。
         * @param Ttl 待检查的有效时长。
         * @param Now 计算过期时间所用的当前时刻。
         * @return 时长可安全加到 `Now` 上时返回 `true`。
         */
        [[nodiscard]] static auto IsTtlRepresentable(
            std::chrono::seconds Ttl,
            std::chrono::steady_clock::time_point Now) noexcept -> bool
        {
            const auto Remaining = std::chrono::duration_cast<
                std::chrono::seconds>(
                std::chrono::steady_clock::time_point::max() - Now);
            return Ttl > std::chrono::seconds::zero()
                && Ttl <= Remaining;
        }

        /**
         * @brief 验证存储资源上限。
         * @throws std::invalid_argument 任一资源上限为零时抛出。
         */
        void ValidateConfiguration() const
        {
            if (MemberConfig.MaxSessions == 0
                || MemberConfig.MaxValuesPerSession == 0
                || MemberConfig.MaxKeySize == 0
                || MemberConfig.MaxValueSize == 0)
            {
                throw std::invalid_argument(
                    "session resource limits must be positive");
            }
        }

        /**
         * @brief 删除指定时刻已过期的 Session。
         * @param Now 判断过期所用的时刻。
         * @note 调用方必须持有 `MemberMutex`；扫描复杂度为 O(n)。
         */
        void CleanupExpiredLocked(
            std::chrono::steady_clock::time_point Now)
        {
            for (auto It = MemberSessions.begin();
                 It != MemberSessions.end();)
            {
                if (Now >= It->second.Expires)
                {
                    It = MemberSessions.erase(It);
                }
                else
                {
                    ++It;
                }
            }
        }

        /**
         * @brief 生成随机 Session 标识。
         * @return 以 `sess_` 为前缀、后接随机字节十六进制编码的标识。
         */
        auto GenerateId() -> std::string
        {
            CryptoPP::AutoSeededRandomPool Rng;
            std::array<std::uint8_t, 32> Bytes{};
            Rng.GenerateBlock(Bytes.data(), Bytes.size());

            std::ostringstream Stream;
            Stream << "sess_" << std::hex << std::setfill('0');
            for (const auto Byte : Bytes)
            {
                Stream << std::setw(2)
                       << static_cast<unsigned int>(Byte);
            }
            return Stream.str();
        }

        /** @brief 创建后保持不变的资源上限配置。 */
        SessionConfig MemberConfig;

        /** @brief 保护 Session 表及其数据的互斥量。 */
        std::mutex MemberMutex;

        /** @brief 以 Session 标识为键的内存存储。 */
        std::unordered_map<std::string, SessionData> MemberSessions;
    };
}
