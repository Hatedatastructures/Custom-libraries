/**
 * @file Application.hpp
 * @brief Web 应用生命周期封装。
 */
#pragma once

#include "Wan/Web/Core/Metrics.hpp"
#include "Wan/Web/Server/Server.hpp"
#include "Wan/Web/Server/TlsServer.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace Wan::Web
{
    /** @brief Web 应用的生命周期状态。 */
    enum class ApplicationState : std::uint8_t
    {
        /** @brief 尚未启动。 */
        Created,
        /** @brief 正在启动。 */
        Starting,
        /** @brief 正常运行。 */
        Running,
        /** @brief 正在停止。 */
        Stopping,
        /** @brief 已停止。 */
        Stopped,
        /** @brief 启动或运行失败。 */
        Failed
    };

    /** @brief Web 应用配置。 */
    struct ApplicationConfig
    {
        /** @brief 底层 HTTP 服务器配置。 */
        ServerConfig Server;
        /** @brief 请求优雅停止后的最长等待时间。 */
        std::chrono::seconds DrainTimeout{10};
        /** @brief 设置时由 Application 托管 TLS Server；未设置时使用普通 HTTP。 */
        std::optional<TlsServerConfig> Tls;
    };

    /**
     * @brief 管理 Web 服务器的启动、停止和失败状态。
     * @details 对象拥有底层服务器；析构时会请求停止并等待清理完成。
     */
    class Application
    {
    public:
        /**
         * @brief 使用指定配置创建应用。
         * @param Config 应用及底层服务器配置。
         */
        explicit Application(ApplicationConfig Config = {});

        /** @brief 请求停止服务并等待清理；析构期间不向外传播异常。 */
        ~Application();

        /** @brief 禁止复制构造。 */
        Application(const Application&) = delete;
        /** @brief 禁止复制赋值。 */
        Application& operator=(const Application&) = delete;
        /** @brief 禁止移动构造。 */
        Application(Application&&) = delete;
        /** @brief 禁止移动赋值。 */
        Application& operator=(Application&&) = delete;

        /**
         * @brief 获取底层服务器。
         * @return 对所拥有服务器的可写引用。
         */
        [[nodiscard]] auto GetServer() noexcept -> Server&;
        /**
         * @brief 以只读引用获取底层服务器。
         * @return 对所拥有服务器的只读引用。
         */
        [[nodiscard]] auto GetServer() const noexcept -> const Server&;
        /**
         * @brief 获取服务器指标。
         * @return 服务器指标的共享指针。
         */
        [[nodiscard]] auto GetMetrics() const noexcept
            -> std::shared_ptr<ServerMetrics>;
        /**
         * @brief 获取当前生命周期状态。
         * @return 当前状态。
         */
        [[nodiscard]] auto State() const noexcept -> ApplicationState;

        /**
         * @brief 启动应用。
         * @throws std::logic_error 当前状态不允许启动时抛出。
         * @note 应用已运行时重复调用不会产生效果。
         */
        void Start();

        /**
         * @brief 请求优雅停止；尚未启动的应用会直接进入已停止状态。
         * @throws std::invalid_argument 停止等待时长无法由计时器表示时抛出。
         */
        void Stop();

        /**
         * @brief 等待停止和资源清理完成。
         * @note 启动或运行期间记录的异常会向调用方重新抛出。
         */
        void Wait();

    private:
        /** @brief 应用停止时长和服务器资源限制配置。 */
        ApplicationConfig MemberConfig;
        /** @brief 由应用生命周期管理的服务器实例。 */
        std::shared_ptr<Server> MemberServer;
        /** @brief 当前应用生命周期状态。 */
        ApplicationState MemberState = ApplicationState::Created;
        /** @brief 保护应用状态及相关生命周期操作的互斥量。 */
        mutable std::mutex MemberLifecycleMutex;
        /** @brief 最近一次启动或运行失败的异常。 */
        std::exception_ptr MemberFailure;

        /**
         * @brief 检查超时和服务器资源上限是否可用。
         * @throws std::invalid_argument 超时为负、超出计时器范围或资源上限为零时抛出。
         */
        void ValidateConfiguration() const;

        /**
         * @brief 根据配置创建普通或 TLS 服务器。
         * @param Config 应用的监听与 TLS 配置。
         * @return 与应用配置匹配的服务器实例。
         */
        [[nodiscard]] static auto CreateServer(
            const ApplicationConfig& Config) -> std::shared_ptr<Server>;
    };
}

namespace Wan::Web
{
    inline Application::Application(ApplicationConfig Config)
        : MemberConfig(std::move(Config))
        , MemberServer(CreateServer(MemberConfig))
    {
    }

    inline auto Application::CreateServer(const ApplicationConfig& Config)
        -> std::shared_ptr<Server>
    {
        if (Config.Tls)
        {
            return std::make_shared<TlsServer>(Config.Server, *Config.Tls);
        }
        return std::make_shared<Server>(Config.Server);
    }

    inline Application::~Application()
    {
        try
        {
            Stop();
            Wait();
        }
        catch (...)
        {
        }
    }

    inline auto Application::GetServer() noexcept -> Server&
    {
        return *MemberServer;
    }

    inline auto Application::GetServer() const noexcept -> const Server&
    {
        return *MemberServer;
    }

    inline auto Application::GetMetrics() const noexcept
        -> std::shared_ptr<ServerMetrics>
    {
        return MemberServer->GetMetrics();
    }

    inline auto Application::State() const noexcept -> ApplicationState
    {
        std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
        return MemberState;
    }

    inline void Application::Start()
    {
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            if (MemberState == ApplicationState::Running)
            {
                return;
            }
            if (MemberState != ApplicationState::Created)
            {
                throw std::logic_error(
                    "application cannot start from its current state");
            }
            MemberState = ApplicationState::Starting;
        }

        try
        {
            ValidateConfiguration();
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            if (MemberState == ApplicationState::Stopping)
            {
                MemberState = ApplicationState::Stopped;
                return;
            }
            if (MemberState != ApplicationState::Starting)
            {
                throw std::logic_error(
                    "application start was superseded by another lifecycle operation");
            }
            MemberServer->Start();
            MemberState = ApplicationState::Running;
        }
        catch (...)
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            MemberFailure = std::current_exception();
            MemberState = ApplicationState::Failed;
            throw;
        }
    }

    inline void Application::Stop()
    {
        ApplicationState StateValue;
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            StateValue = MemberState;
            if (StateValue == ApplicationState::Created)
            {
                MemberState = ApplicationState::Stopped;
                return;
            }
            if (StateValue == ApplicationState::Stopped
                || StateValue == ApplicationState::Stopping)
            {
                return;
            }
            if (StateValue != ApplicationState::Failed)
            {
                MemberState = ApplicationState::Stopping;
            }
        }

        if (StateValue == ApplicationState::Failed)
        {
            MemberServer->ForceStop();
            return;
        }

        try
        {
            MemberServer->BeginGracefulStop(MemberConfig.DrainTimeout);
        }
        catch (...)
        {
            {
                std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                MemberFailure = std::current_exception();
                MemberState = ApplicationState::Failed;
            }
            MemberServer->ForceStop();
            throw;
        }
    }

    inline void Application::Wait()
    {
        std::exception_ptr KnownFailure;
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            if (MemberState == ApplicationState::Created
                || MemberState == ApplicationState::Stopped)
            {
                if (MemberState == ApplicationState::Created)
                {
                    MemberState = ApplicationState::Stopped;
                }
                return;
            }
            if (MemberState == ApplicationState::Failed)
            {
                KnownFailure = MemberFailure;
            }
        }

        if (KnownFailure)
        {
            try
            {
                MemberServer->Wait();
            }
            catch (...)
            {
            }
            std::rethrow_exception(KnownFailure);
        }

        try
        {
            MemberServer->Wait();
        }
        catch (...)
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            MemberFailure = std::current_exception();
            MemberState = ApplicationState::Failed;
            throw;
        }

        std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
        if (MemberState == ApplicationState::Starting
            || MemberState == ApplicationState::Running
            || MemberState == ApplicationState::Stopping)
        {
            MemberState = ApplicationState::Stopped;
        }
    }

    inline void Application::ValidateConfiguration() const
    {
        const auto MaximumTimerSeconds = std::chrono::duration_cast<
            std::chrono::seconds>(std::chrono::steady_clock::duration::max());
        const auto IsTimerDurationRepresentable =
            [MaximumTimerSeconds](std::chrono::seconds Value) noexcept
            {
                return Value >= std::chrono::seconds::zero()
                    && Value <= MaximumTimerSeconds;
            };

        if (MemberConfig.DrainTimeout < std::chrono::seconds::zero()
            || MemberConfig.Server.ReadTimeout < std::chrono::seconds::zero()
            || MemberConfig.Server.WriteTimeout < std::chrono::seconds::zero()
            || MemberConfig.Server.KeepAliveTimeout
                < std::chrono::seconds::zero()
            || !IsTimerDurationRepresentable(MemberConfig.DrainTimeout)
            || !IsTimerDurationRepresentable(MemberConfig.Server.ReadTimeout)
            || !IsTimerDurationRepresentable(
                MemberConfig.Server.WriteTimeout)
            || !IsTimerDurationRepresentable(
                MemberConfig.Server.KeepAliveTimeout))
        {
            throw std::invalid_argument(
                "application timeouts are outside the timer range");
        }
        if (MemberConfig.Server.MaxBodySize == 0
            || MemberConfig.Server.MaxHeaderSize == 0
            || MemberConfig.Server.MaxConnections == 0
            || (MemberConfig.Server.KeepAliveEnabled
                && MemberConfig.Server.MaxKeepaliveRequests == 0))
        {
            throw std::invalid_argument(
                "application resource limits must be positive");
        }
    }
}
