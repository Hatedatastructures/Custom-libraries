/**
 * @file Server.hpp
 * @brief HTTP 服务器
 * @details Accept 循环 + 每连接协程 + Keep-Alive + 中间件。
 *          HTTP 连接直接持有 tcp::socket，不走 transmission 抽象。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>

#include "Wan/Web/Core/Context.hpp"
#include "Wan/Web/Core/Executor.hpp"
#include "Wan/Web/Core/Metrics.hpp"
#include "Wan/Web/Middleware/Middleware.hpp"
#include "Wan/Web/Router/Router.hpp"
#include "Wan/Web/Server/ConnectionRegistry.hpp"

namespace Wan::Web
{
    namespace Net = boost::asio;
    namespace Beast = boost::beast;
    namespace Http = Beast::http;

    /**
     * @struct ServerConfig
     * @brief 服务器配置
     */
    struct ServerConfig
    {
        /** @brief 工作线程数；0 表示使用硬件并发数。 */
        std::size_t Threads = 0;
        /** @brief 请求读取超时。 */
        std::chrono::seconds ReadTimeout{30};
        /** @brief 响应写入超时。 */
        std::chrono::seconds WriteTimeout{30};
        /** @brief Keep-Alive 空闲超时。 */
        std::chrono::seconds KeepAliveTimeout{30};
        /** @brief 请求正文最大字节数。 */
        std::size_t MaxBodySize = 10 * 1024 * 1024;
        /** @brief 同时允许的最大连接数。 */
        std::size_t MaxConnections = 10000;
        /** @brief 是否启用 HTTP Keep-Alive。 */
        bool KeepAliveEnabled = true;
        /** @brief 每个 Keep-Alive 连接允许的最大请求数。 */
        std::size_t MaxKeepaliveRequests = 100;
        /** @brief 是否启用 TCP_NODELAY。 */
        bool TcpNoDelay = true;
        /** @brief 是否启用 SO_REUSEADDR。 */
        bool ReuseAddress = true;
        /** @brief 是否收集服务器指标。 */
        bool EnableMetrics = true;
        /** @brief 请求头最大字节数。 */
        std::size_t MaxHeaderSize = 64 * 1024;
    };

    /**
     * @class Server
     * @brief HTTP 服务器
     * @details 路由和中间件必须在 `Start()` 前配置；服务器由共享指针管理。
     */
    class Server : public std::enable_shared_from_this<Server>
    {
    public:
        /**
         * @brief 创建服务器。
         * @param Config 服务器配置。
         */
        explicit Server(ServerConfig Config = {})
            : MemberConfig(std::move(Config))
            , MemberExecutor(MemberConfig.Threads)
            , MemberAcceptStrand(
                Net::make_strand(MemberExecutor.GetExecutor()))
            , MemberMetrics(std::make_shared<ServerMetrics>())
        {
        }

        /**
         * @brief 注册 GET 路由。
         * @param Pattern 路由路径模式。
         * @param H 请求处理器。
         * @return 当前服务器，供链式配置使用。
         * @throws std::logic_error 服务器已启动时抛出。
         */
        auto Get(std::string_view Pattern, Handler H) -> Server&
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            EnsureConfigurationMutableLocked();
            MemberRouter.Get(Pattern, std::move(H));
            return *this;
        }

        /**
         * @brief 注册 POST 路由。
         * @param Pattern 路由路径模式。
         * @param H 请求处理器。
         * @return 当前服务器，供链式配置使用。
         * @throws std::logic_error 服务器已启动时抛出。
         */
        auto Post(std::string_view Pattern, Handler H) -> Server&
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            EnsureConfigurationMutableLocked();
            MemberRouter.Post(Pattern, std::move(H));
            return *this;
        }

        /**
         * @brief 注册 PUT 路由。
         * @param Pattern 路由路径模式。
         * @param H 请求处理器。
         * @return 当前服务器，供链式配置使用。
         * @throws std::logic_error 服务器已启动时抛出。
         */
        auto Put(std::string_view Pattern, Handler H) -> Server&
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            EnsureConfigurationMutableLocked();
            MemberRouter.Put(Pattern, std::move(H));
            return *this;
        }

        /**
         * @brief 注册 DELETE 路由。
         * @param Pattern 路由路径模式。
         * @param H 请求处理器。
         * @return 当前服务器，供链式配置使用。
         * @throws std::logic_error 服务器已启动时抛出。
         */
        auto Del(std::string_view Pattern, Handler H) -> Server&
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            EnsureConfigurationMutableLocked();
            MemberRouter.Del(Pattern, std::move(H));
            return *this;
        }

        /**
         * @brief 注册 PATCH 路由。
         * @param Pattern 路由路径模式。
         * @param H 请求处理器。
         * @return 当前服务器，供链式配置使用。
         * @throws std::logic_error 服务器已启动时抛出。
         */
        auto Patch(std::string_view Pattern, Handler H) -> Server&
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            EnsureConfigurationMutableLocked();
            MemberRouter.Patch(Pattern, std::move(H));
            return *this;
        }

        /**
         * @brief 添加中间件。
         * @param M 中间件对象。
         * @return 当前服务器，供链式配置使用。
         * @throws std::logic_error 服务器已启动时抛出。
         */
        auto Use(Middleware M) -> Server&
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            EnsureConfigurationMutableLocked();
            MemberChain.Use(std::move(M));
            return *this;
        }

        /**
         * @brief 添加带优先级和名称的中间件函数。
         * @param Fn 中间件函数。
         * @param Priority 执行优先级。
         * @param Name 中间件名称。
         * @return 当前服务器，供链式配置使用。
         * @throws std::logic_error 服务器已启动时抛出。
         */
        auto Use(MiddlewareFn Fn, int Priority = 0,
            std::string Name = "") -> Server&
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            EnsureConfigurationMutableLocked();
            MemberChain.Use(std::move(Fn), Priority, std::move(Name));
            return *this;
        }

        /**
         * @brief 设置监听端口。
         * @param Port TCP 端口。
         * @return 当前服务器，供链式配置使用。
         * @throws std::logic_error 服务器已启动时抛出。
         */
        auto Listen(std::uint16_t Port) -> Server&
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            EnsureConfigurationMutableLocked();
            MemberPort = Port;
            return *this;
        }

        /**
         * @brief 启动服务器并等待监听器就绪。
         * @throws std::logic_error 服务器已启动时抛出。
         * @throws std::invalid_argument 配置无效时抛出。
         * @note 服务器或其派生对象必须由 `std::shared_ptr` 管理。
         */
        void Start();

        /**
         * @brief 等待服务器工作线程退出并完成连接清理。
         * @note 启动、运行或工作处理器中的异常会在此处重新抛出。
         */
        void Wait();

        /**
         * @brief 停止接收新连接，并在超时前等待活动连接结束。
         * @param DrainTimeout 优雅关闭的等待时长。
         * @throws std::invalid_argument 超出计时器可表示范围时抛出。
         */
        void BeginGracefulStop(std::chrono::seconds DrainTimeout);

        /**
         * @brief 立即取消活动连接并请求停止工作线程。
         */
        void ForceStop();

        /**
         * @brief 返回运行服务器 accept 循环的协程。
         * @return accept 循环对应的 Asio awaitable。
         * @note 通常应调用 `Start()`，而不是直接运行此协程。
         */
        auto Run() -> Net::awaitable<void>
        {
            auto Ex = co_await Net::this_coro::executor;
            auto Self = shared_from_this();

            try
            {
                const Net::ip::tcp::endpoint Endpoint{
                    Net::ip::tcp::v4(), MemberPort};
                auto Acceptor = std::make_shared<Net::ip::tcp::acceptor>(Ex);
                boost::system::error_code OpenEc;
                OpenEc = Acceptor->open(Endpoint.protocol(), OpenEc);
                if (OpenEc)
                {
                    throw boost::system::system_error(OpenEc);
                }

                if (MemberConfig.ReuseAddress)
                {
                    OpenEc = Acceptor->set_option(
                        Net::socket_base::reuse_address{true}, OpenEc);
                    if (OpenEc)
                    {
                        throw boost::system::system_error(OpenEc);
                    }
                }

                OpenEc = Acceptor->bind(Endpoint, OpenEc);
                if (OpenEc)
                {
                    throw boost::system::system_error(OpenEc);
                }
                OpenEc = Acceptor->listen(
                    Net::socket_base::max_listen_connections, OpenEc);
                if (OpenEc)
                {
                    throw boost::system::system_error(OpenEc);
                }

                {
                    std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                    if (MemberStopping)
                    {
                        MemberStartFailure = std::make_exception_ptr(
                            std::logic_error(
                                "server stopped during startup"));
                    }
                    else
                    {
                        MemberAcceptor = Acceptor;
                    }
                    MemberStartComplete = true;
                }
                MemberStartCondition.notify_all();

                bool StartupStopping = false;
                {
                    std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                    StartupStopping = MemberStopping;
                }
                if (StartupStopping)
                {
                    boost::system::error_code CloseEc;
                    Acceptor->close(CloseEc);
                    MarkRunComplete();
                    co_return;
                }

                for (;;)
                {
                boost::system::error_code Ec;
                auto ConnectionExecutor = Net::make_strand(
                    MemberExecutor.GetExecutor());
                auto Socket = co_await Acceptor->async_accept(
                    ConnectionExecutor,
                    Net::redirect_error(Net::use_awaitable, Ec));

                bool Stopping = false;
                if (Ec)
                {
                    {
                        std::lock_guard<std::mutex> Lock{
                            MemberLifecycleMutex};
                        Stopping = MemberStopping;
                    }
                    if (Stopping || Ec == Net::error::operation_aborted)
                    {
                        break;
                    }
                    Net::steady_timer RetryTimer{Ex,
                        std::chrono::milliseconds{50}};
                    boost::system::error_code RetryEc;
                    co_await RetryTimer.async_wait(
                        Net::redirect_error(Net::use_awaitable, RetryEc));
                    continue;
                }

                {
                    std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                    Stopping = MemberStopping;
                }
                if (Stopping)
                {
                    boost::system::error_code SocketEc;
                    Socket.close(SocketEc);
                    break;
                }

                if (MemberConfig.TcpNoDelay)
                {
                    boost::system::error_code SocketEc;
                    SocketEc = Socket.set_option(
                        Net::ip::tcp::no_delay{true}, SocketEc);
                    if (SocketEc)
                    {
                        SocketEc = Socket.close(SocketEc);
                        continue;
                    }
                }

                if (!TryAcquireConnection())
                {
                    boost::system::error_code SocketEc;
                    SocketEc = Socket.close(SocketEc);
                    continue;
                }

                {
                    std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                    Stopping = MemberStopping;
                }
                if (Stopping)
                {
                    MemberActiveConnections.fetch_sub(
                        1, std::memory_order_acq_rel);
                    boost::system::error_code SocketEc;
                    Socket.close(SocketEc);
                    break;
                }

                auto Stream = std::make_shared<Beast::tcp_stream>(
                    std::move(Socket));
                auto CancelSignal = std::make_shared<
                    Net::cancellation_signal>();
                auto IsHandlerActive = std::make_shared<std::atomic_bool>(
                    false);
                ConnectionRegistry::Id ConnectionId = 0;
                try
                {
                    ConnectionId = MemberConnections.Register(
                        Stream, CancelSignal, IsHandlerActive);
                }
                catch (...)
                {
                    MemberActiveConnections.fetch_sub(
                        1, std::memory_order_acq_rel);
                    boost::system::error_code SocketEc;
                    SocketEc = Stream->socket().close(SocketEc);
                    continue;
                }
                if (MemberConfig.EnableMetrics)
                {
                    MemberMetrics->IncActiveConnections();
                }

                try
                {
                    Net::co_spawn(Stream->get_executor(),
                        Self->HandleConnection(Self, CancelSignal, Stream,
                            ConnectionId, IsHandlerActive),
                        Net::detached);
                }
                catch (...)
                {
                    MemberConnections.Remove(ConnectionId);
                    MemberActiveConnections.fetch_sub(
                        1, std::memory_order_acq_rel);
                    if (MemberConfig.EnableMetrics)
                    {
                        MemberMetrics->DecActiveConnections();
                    }
                    boost::system::error_code SocketEc;
                    Stream->socket().close(SocketEc);
                    throw;
                }
                }
                MarkRunComplete();
            }
            catch (...)
            {
                std::shared_ptr<Net::ip::tcp::acceptor> Acceptor;
                std::shared_ptr<Net::steady_timer> DrainTimer;
                {
                    std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                    if (!MemberStartComplete)
                    {
                        MemberStartFailure = std::current_exception();
                        MemberStartComplete = true;
                    }
                    else if (!MemberStopping)
                    {
                        MemberRunFailure = std::current_exception();
                    }
                    MemberStopping = true;
                    MemberForceStopRequested = true;
                    Acceptor = MemberAcceptor;
                    MemberAcceptor.reset();
                    DrainTimer = MemberDrainTimer;
                    MemberRunComplete = true;
                }
                MemberStartCondition.notify_all();
                CloseAcceptor(std::move(Acceptor));
                CancelDrainTimer(DrainTimer);
                MemberConnections.CloseAll();
                MaybeFinishGracefulStop();
            }
        }

        /**
         * @brief 启动服务器并阻塞至工作线程退出。
         * @note `Start()` 或 `Wait()` 捕获的异常会向调用方传播。
         */
        void RunSync()
        {
            Start();
            Wait();
        }

        /**
         * @brief 请求立即停止服务器；等同于 `ForceStop()`。
         */
        void Stop()
        {
            ForceStop();
        }

        /**
         * @brief 获取服务器指标对象。
         * @return 共享的指标对象。
         */
        [[nodiscard]] auto GetMetrics() const
            -> std::shared_ptr<ServerMetrics>
        {
            return MemberMetrics;
        }

        /**
         * @brief 获取服务器路由器。
         * @return 路由器引用；服务器启动后路由配置已冻结。
         */
        [[nodiscard]] auto GetRouter() -> Router&
        {
            return MemberRouter;
        }

    private:
        /**
         * @brief 处理一个 TCP 连接上的请求/响应与 keep-alive 循环。
         * @param KeepAlive 保持服务器实例存活的共享指针。
         * @param Cancellation 连接处理器的取消信号。
         * @param Socket 当前连接的 TCP 流。
         * @param ConnectionId 连接注册表中的条目标识。
         * @param IsHandlerActive 连接处理器等待状态的共享标志。
         * @return 连接处理并清理完成时结束的 awaitable。
         */
        auto HandleConnection(
            std::shared_ptr<Server> KeepAlive,
            std::shared_ptr<Net::cancellation_signal> Cancellation,
            std::shared_ptr<Beast::tcp_stream> Socket,
            ConnectionRegistry::Id ConnectionId,
            ConnectionRegistry::HandlerActive IsHandlerActive)
            -> Net::awaitable<void>
        {
            (void)KeepAlive;
            (void)Cancellation;
            Beast::flat_buffer Buffer;
            std::size_t RequestCount = 0;
            const auto HandlerExecutor =
                co_await Net::this_coro::executor;

            auto Close = [&]() -> Net::awaitable<void>
            {
                boost::system::error_code Ec;
                Ec = Socket->socket().shutdown(
                    Net::ip::tcp::socket::shutdown_send, Ec);
                Ec = Socket->socket().close(Ec);
                MemberConnections.Remove(ConnectionId);
                MemberActiveConnections.fetch_sub(
                    1, std::memory_order_acq_rel);
                if (MemberConfig.EnableMetrics)
                {
                    MemberMetrics->DecActiveConnections();
                }
                MaybeFinishGracefulStop();
                co_return;
            };

            bool Failed = false;
            try
            {
                for (;;)
                {
                    const auto ReadTimeout = RequestCount == 0
                        ? MemberConfig.ReadTimeout
                        : (MemberConfig.KeepAliveEnabled
                            ? MemberConfig.KeepAliveTimeout
                            : MemberConfig.ReadTimeout);
                    if (ReadTimeout.count() > 0)
                    {
                        Socket->expires_after(ReadTimeout);
                    }
                    else
                    {
                        Socket->expires_never();
                    }

                    Http::request_parser<Http::string_body> Parser;
                    Parser.header_limit(MemberConfig.MaxHeaderSize);
                    Parser.body_limit(MemberConfig.MaxBodySize);
                    boost::system::error_code Ec;
                    co_await Http::async_read(*Socket, Buffer, Parser,
                        Net::redirect_error(Net::use_awaitable, Ec));
                    Socket->expires_never();

                    if (Ec)
                    {
                        if (Ec != Http::error::end_of_stream
                            && MemberConfig.EnableMetrics)
                        {
                            MemberMetrics->IncErrors();
                        }
                        break;
                    }

                    auto Req = Parser.release();
                    if (MemberConfig.EnableMetrics)
                    {
                        MemberMetrics->IncRequests();
                        MemberMetrics->IncBytesRead(Req.body().size());
                    }

                // 构造上下文
                Context Ctx{std::move(Req)};

                // 路由匹配
                    auto Match = MemberRouter.Match(
                        Ctx.Method(), Ctx.Target());

                    if (Match)
                    {
                        Ctx.SetParams(std::move(Match->Params));

                        if (MemberChain.Empty())
                        {
                            IsHandlerActive->store(
                                true, std::memory_order_release);
                            try
                            {
                                co_await Net::co_spawn(HandlerExecutor,
                                    Match->Handler(Ctx),
                                    Net::bind_cancellation_slot(
                                        Cancellation->slot(),
                                        Net::use_awaitable));
                            }
                            catch (...)
                            {
                                IsHandlerActive->store(
                                    false, std::memory_order_release);
                                throw;
                            }
                            IsHandlerActive->store(
                                false, std::memory_order_release);
                        }
                        else
                        {
                            IsHandlerActive->store(
                                true, std::memory_order_release);
                            try
                            {
                                co_await Net::co_spawn(HandlerExecutor,
                                    MemberChain.Run(
                                        Ctx, std::move(Match->Handler)),
                                    Net::bind_cancellation_slot(
                                        Cancellation->slot(),
                                        Net::use_awaitable));
                            }
                            catch (...)
                            {
                                IsHandlerActive->store(
                                    false, std::memory_order_release);
                                throw;
                            }
                            IsHandlerActive->store(
                                false, std::memory_order_release);
                        }
                    }
                    else
                    {
                        Ctx.Status(Http::status::not_found);
                        Ctx.Json(R"({"error":"not_found"})");
                    }

                // 发送响应
                    Ctx.Response().keep_alive(Ctx.KeepAlive());
                    Ctx.Response().prepare_payload();

                    if (MemberConfig.WriteTimeout.count() > 0)
                    {
                        Socket->expires_after(MemberConfig.WriteTimeout);
                    }
                    else
                    {
                        Socket->expires_never();
                    }
                    co_await Http::async_write(*Socket, Ctx.Response(),
                        Net::redirect_error(Net::use_awaitable, Ec));
                    Socket->expires_never();

                    if (Ec)
                    {
                        if (MemberConfig.EnableMetrics)
                        {
                            MemberMetrics->IncErrors();
                        }
                        break;
                    }

                    if (MemberConfig.EnableMetrics)
                    {
                        MemberMetrics->IncBytesWritten(
                            Ctx.Response().body().size());
                    }

                // Keep-Alive 检查
                    ++RequestCount;
                    if (!MemberConfig.KeepAliveEnabled || !Ctx.KeepAlive()
                        || RequestCount >= MemberConfig.MaxKeepaliveRequests)
                    {
                        break;
                    }
                }
            }
            catch (...)
            {
                if (MemberConfig.EnableMetrics)
                {
                    MemberMetrics->IncErrors();
                }
                Failed = true;
            }

            if (Failed)
            {
                co_await Close();
                co_return;
            }
            co_await Close();
        }

        /** @brief 监听、超时、keep-alive 和资源上限配置。 */
        ServerConfig MemberConfig;
        /** @brief 执行 accept、连接处理器和定时器协程的执行器。 */
        Executor MemberExecutor;
        /** @brief 串行化 acceptor 和连接生命周期操作的 strand。 */
        Net::strand<Net::any_io_executor> MemberAcceptStrand;
        /** @brief 服务器路由表。 */
        Router MemberRouter;
        /** @brief 处理请求时依次调用的中间件链。 */
        MiddlewareChain MemberChain;
        /** @brief 启用指标时共享的服务器指标对象。 */
        std::shared_ptr<ServerMetrics> MemberMetrics;
        /** @brief 当前活动连接数的原子计数。 */
        std::atomic<std::size_t> MemberActiveConnections{0};
        /** @brief 当前监听端口。 */
        std::uint16_t MemberPort = 8080;
        /** @brief 跟踪活动连接并负责取消/关闭连接。 */
        ConnectionRegistry MemberConnections;
        /** @brief 当前监听器；停止或未启动时为空。 */
        std::shared_ptr<Net::ip::tcp::acceptor> MemberAcceptor;
        /** @brief 保护服务器生命周期状态的互斥量。 */
        mutable std::mutex MemberLifecycleMutex;
        /** @brief 串行化 Wait() 调用的互斥量。 */
        mutable std::mutex MemberWaitMutex;
        /** @brief 通知启动完成或失败的条件变量。 */
        std::condition_variable MemberStartCondition;
        /** @brief 是否已成功开始启动流程。 */
        bool MemberStarted = false;
        /** @brief 监听器启动结果是否已确定。 */
        bool MemberStartComplete = false;
        /** @brief 是否正在停止服务器。 */
        bool MemberStopping = false;
        /** @brief Run() 协程是否已结束。 */
        bool MemberRunComplete = false;
        /** @brief 是否已请求强制停止。 */
        bool MemberForceStopRequested = false;
        /** @brief 优雅停止定时器是否仍在等待完成回调。 */
        bool MemberDrainTimerActive = false;
        /** @brief 当前优雅停止定时器。 */
        std::shared_ptr<Net::steady_timer> MemberDrainTimer;
        /** @brief 启动阶段捕获的异常。 */
        std::exception_ptr MemberStartFailure;
        /** @brief 运行阶段捕获的异常。 */
        std::exception_ptr MemberRunFailure;

        /**
         * @brief 确认启动后不再修改配置。
         * @pre 调用方持有 MemberLifecycleMutex。
         * @throws std::logic_error 服务器已启动时抛出。
         */
        void EnsureConfigurationMutableLocked() const
        {
            if (MemberStarted)
            {
                throw std::logic_error(
                    "server configuration cannot change after start");
            }
        }

        /**
         * @brief 保持服务器实例存活并运行主 accept 协程。
         * @param Self 保持存活并由协程运行的服务器实例。
         * @return 完成 accept 循环的 awaitable。
         */
        [[nodiscard]] static auto RunWithKeepAlive(
            std::shared_ptr<Server> Self) -> Net::awaitable<void>
        {
            co_await Self->Run();
        }

        /**
         * @brief 等待优雅停止期限；超时时请求强制停止。
         * @param Self 保持服务器实例存活的共享指针。
         * @param Timer 优雅停止期限计时器。
         * @return 定时器结束并完成清理后的 awaitable。
         * @note awaitable 异常会在完成定时器状态清理后重新抛出。
         */
        [[nodiscard]] static auto StopAfterDrainTimeout(
            std::shared_ptr<Server> Self,
            std::shared_ptr<Net::steady_timer> Timer) -> Net::awaitable<void>
        {
            boost::system::error_code Ec;
            try
            {
                if (!Self->ForceStopRequested())
                {
                    co_await Timer->async_wait(
                        Net::redirect_error(Net::use_awaitable, Ec));
                }
                if (!Ec && !Self->ForceStopRequested())
                {
                    Self->ForceStop();
                }
            }
            catch (...)
            {
                Self->CompleteDrainTimer(Timer);
                throw;
            }
            Self->CompleteDrainTimer(Timer);
        }

        /**
         * @brief 在线程安全地读取强制停止请求状态。
         * @return 已请求强制停止时返回 `true`。
         */
        [[nodiscard]] auto ForceStopRequested() const -> bool
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            return MemberForceStopRequested;
        }

        /**
         * @brief 在 accept strand 上取消指定的优雅停止计时器。
         * @param Timer 要取消的计时器；空指针时不执行操作。
         */
        void CancelDrainTimer(
            const std::shared_ptr<Net::steady_timer>& Timer)
        {
            if (Timer)
            {
                Net::dispatch(MemberAcceptStrand, [Timer]
                {
                    (void)Timer->cancel();
                });
            }
        }

        /**
         * @brief 清除完成的优雅停止计时器状态并检查是否可结束停止流程。
         * @param Timer 已完成等待的计时器。
         */
        void CompleteDrainTimer(
            const std::shared_ptr<Net::steady_timer>& Timer)
        {
            {
                std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                if (MemberDrainTimer == Timer)
                {
                    MemberDrainTimer.reset();
                }
                MemberDrainTimerActive = false;
            }
            MaybeFinishGracefulStop();
        }

        /**
         * @brief 在 accept strand 上关闭监听器。
         * @param Acceptor 要关闭的监听器；空指针时不执行操作。
         */
        void CloseAcceptor(
            std::shared_ptr<Net::ip::tcp::acceptor> Acceptor)
        {
            if (!Acceptor)
            {
                return;
            }
            Net::dispatch(MemberAcceptStrand,
                [Acceptor = std::move(Acceptor)]() mutable
                {
                    boost::system::error_code Ec;
                    Acceptor->close(Ec);
                });
        }

        /** @brief 标记主运行协程结束并推进停止完成检查。 */
        void MarkRunComplete()
        {
            {
                std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                MemberRunComplete = true;
            }
            MaybeFinishGracefulStop();
        }

        /**
         * @brief 检查秒数能否由 `steady_timer` 时长表示。
         * @param Value 待检查的秒数。
         * @return `Value` 为非负数且可表示时返回 `true`。
         */
        [[nodiscard]] static auto IsTimerDurationRepresentable(
            std::chrono::seconds Value) noexcept -> bool
        {
            const auto Maximum = std::chrono::duration_cast<
                std::chrono::seconds>(Net::steady_timer::duration::max());
            return Value >= std::chrono::seconds::zero()
                && Value <= Maximum;
        }

        /**
         * @brief 校验超时和服务资源限制配置。
         * @throws std::invalid_argument 超时为负或不可表示，或资源限制为零时抛出。
         */
        void ValidateConfiguration() const
        {
            if (MemberConfig.ReadTimeout < std::chrono::seconds::zero()
                || !IsTimerDurationRepresentable(MemberConfig.ReadTimeout))
            {
                throw std::invalid_argument(
                    "server read timeout is outside the timer range");
            }
            if (MemberConfig.WriteTimeout < std::chrono::seconds::zero()
                || !IsTimerDurationRepresentable(MemberConfig.WriteTimeout))
            {
                throw std::invalid_argument(
                    "server write timeout is outside the timer range");
            }
            if (MemberConfig.KeepAliveTimeout
                    < std::chrono::seconds::zero()
                || !IsTimerDurationRepresentable(
                    MemberConfig.KeepAliveTimeout))
            {
                throw std::invalid_argument(
                    "server keep-alive timeout is outside the timer range");
            }
            if (MemberConfig.MaxBodySize == 0
                || MemberConfig.MaxHeaderSize == 0
                || MemberConfig.MaxConnections == 0
                || (MemberConfig.KeepAliveEnabled
                    && MemberConfig.MaxKeepaliveRequests == 0))
            {
                throw std::invalid_argument(
                    "server resource limits must be positive");
            }
        }

        /**
         * @brief 在未达到连接上限时原子地增加活动连接计数。
         * @return 成功占用连接名额时返回 true。
         */
        [[nodiscard]] auto TryAcquireConnection() noexcept -> bool
        {
            auto Current = MemberActiveConnections.load(
                std::memory_order_acquire);
            while (Current < MemberConfig.MaxConnections)
            {
                if (MemberActiveConnections.compare_exchange_weak(Current,
                        Current + 1, std::memory_order_acq_rel,
                        std::memory_order_acquire))
                {
                    return true;
                }
            }
            return false;
        }

        /**
         * @brief 检查优雅停止是否已满足完成条件。
         * @details 停止请求、主运行协程结束且活动连接归零时，取消排空定时器并停止执行器。
         */
        void MaybeFinishGracefulStop();
    };

    inline void Server::Start()
    {
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            if (MemberStarted)
            {
                throw std::logic_error("server has already been started");
            }
            ValidateConfiguration();
            MemberRouter.Freeze();
            MemberStarted = true;
            MemberStartComplete = false;
            MemberStopping = false;
            MemberRunComplete = false;
            MemberForceStopRequested = false;
            MemberDrainTimerActive = false;
            MemberDrainTimer.reset();
            MemberStartFailure = nullptr;
            MemberRunFailure = nullptr;
        }

        try
        {
            auto Self = shared_from_this();
            Net::any_io_executor AcceptExecutor{MemberAcceptStrand};
            MemberExecutor.Spawn(std::move(AcceptExecutor),
                RunWithKeepAlive(std::move(Self)));
            MemberExecutor.Run();
        }
        catch (...)
        {
            ForceStop();
            try
            {
                Wait();
            }
            catch (...)
            {
            }
            throw;
        }

        std::exception_ptr StartFailure;
        {
            std::unique_lock<std::mutex> Lock{MemberLifecycleMutex};
            MemberStartCondition.wait(Lock,
                [this] { return MemberStartComplete; });
            StartFailure = MemberStartFailure;
        }
        if (StartFailure)
        {
            ForceStop();
            try
            {
                Wait();
            }
            catch (...)
            {
            }
            std::rethrow_exception(StartFailure);
        }
    }

    inline void Server::Wait()
    {
        std::lock_guard<std::mutex> WaitLock{MemberWaitMutex};
        {
            std::unique_lock<std::mutex> Lock{MemberLifecycleMutex};
            if (MemberStarted && !MemberStartComplete)
            {
                MemberStartCondition.wait(Lock,
                    [this] { return MemberStartComplete; });
            }
        }
        std::exception_ptr ExecutorFailure;
        try
        {
            MemberExecutor.Join();
        }
        catch (...)
        {
            ExecutorFailure = std::current_exception();
        }
        std::shared_ptr<Net::ip::tcp::acceptor> Acceptor;
        std::shared_ptr<Net::steady_timer> DrainTimer;
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            MemberStopping = true;
            MemberForceStopRequested = true;
            Acceptor = MemberAcceptor;
            MemberAcceptor.reset();
            DrainTimer = MemberDrainTimer;
        }
        if (Acceptor)
        {
            boost::system::error_code Ec;
            Acceptor->close(Ec);
        }
        if (DrainTimer)
        {
            (void)DrainTimer->cancel();
        }
        MemberConnections.CloseAllNow();
        std::exception_ptr DrainFailure;
        try
        {
            MemberExecutor.DrainStopped();
        }
        catch (...)
        {
            DrainFailure = std::current_exception();
        }
        MemberActiveConnections.store(0, std::memory_order_release);
        MemberMetrics->ResetActiveConnections();

        std::exception_ptr Failure;
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            Failure = MemberRunFailure ? MemberRunFailure
                : MemberStartFailure ? MemberStartFailure
                : ExecutorFailure ? ExecutorFailure : DrainFailure;
        }
        if (Failure)
        {
            std::rethrow_exception(Failure);
        }
    }

    inline void Server::BeginGracefulStop(std::chrono::seconds DrainTimeout)
    {
        if (!IsTimerDurationRepresentable(DrainTimeout))
        {
            throw std::invalid_argument(
                "server drain timeout is outside the timer range");
        }

        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            if (!MemberStarted || MemberStopping)
            {
                return;
            }
        }

        auto DrainTimer = std::make_shared<Net::steady_timer>(
            MemberAcceptStrand, DrainTimeout);
        std::shared_ptr<Net::ip::tcp::acceptor> Acceptor;
        bool NotifyStart = false;
        bool ScheduleDrainTimer = false;
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            if (!MemberStarted || MemberStopping)
            {
                return;
            }
            MemberStopping = true;
            Acceptor = MemberAcceptor;
            if (MemberActiveConnections.load(std::memory_order_acquire) > 0)
            {
                MemberDrainTimer = DrainTimer;
                MemberDrainTimerActive = true;
                ScheduleDrainTimer = true;
            }
            if (!MemberStartComplete)
            {
                MemberStartFailure = std::make_exception_ptr(
                    std::logic_error("server stopped during startup"));
                MemberStartComplete = true;
                NotifyStart = true;
            }
        }

        if (NotifyStart)
        {
            MemberStartCondition.notify_all();
        }

        CloseAcceptor(std::move(Acceptor));

        if (!ScheduleDrainTimer)
        {
            MaybeFinishGracefulStop();
            return;
        }

        try
        {
            auto Self = shared_from_this();
            Net::any_io_executor DrainExecutor{MemberAcceptStrand};
            MemberExecutor.Spawn(std::move(DrainExecutor),
                StopAfterDrainTimeout(std::move(Self), DrainTimer));
        }
        catch (...)
        {
            CompleteDrainTimer(DrainTimer);
            ForceStop();
            throw;
        }
    }

    inline void Server::ForceStop()
    {
        std::shared_ptr<Net::ip::tcp::acceptor> Acceptor;
        std::shared_ptr<Net::steady_timer> DrainTimer;
        bool NotifyStart = false;
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            if (!MemberStarted)
            {
                return;
            }
            MemberStopping = true;
            MemberForceStopRequested = true;
            Acceptor = MemberAcceptor;
            MemberAcceptor.reset();
            DrainTimer = MemberDrainTimer;
            if (MemberStarted && !MemberStartComplete)
            {
                MemberStartFailure = std::make_exception_ptr(
                    std::logic_error("server stopped during startup"));
                MemberStartComplete = true;
                NotifyStart = true;
            }
        }
        if (NotifyStart)
        {
            MemberStartCondition.notify_all();
        }
        CancelDrainTimer(DrainTimer);
        CloseAcceptor(std::move(Acceptor));
        MemberConnections.CloseAll();
        MaybeFinishGracefulStop();
    }

    inline void Server::MaybeFinishGracefulStop()
    {
        bool ShouldStop = false;
        std::shared_ptr<Net::steady_timer> DrainTimer;
        {
            std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
            const bool IsDrained = MemberStopping
                && MemberRunComplete
                && MemberActiveConnections.load(std::memory_order_acquire) == 0;
            if (IsDrained && MemberDrainTimerActive)
            {
                DrainTimer = MemberDrainTimer;
            }
            else
            {
                ShouldStop = IsDrained;
            }
        }
        CancelDrainTimer(DrainTimer);
        if (ShouldStop)
        {
            MemberExecutor.Stop();
        }
    }

    /**
     * @brief 创建使用默认配置的服务器。
     * @return 服务器共享指针。
     */
    [[nodiscard]] inline auto MakeServer()
        -> std::shared_ptr<Server>
    {
        return std::make_shared<Server>();
    }

    /**
     * @brief 使用指定配置创建服务器。
     * @param Config 服务器配置。
     * @return 服务器共享指针。
     */
    [[nodiscard]] inline auto MakeServer(const ServerConfig& Config)
        -> std::shared_ptr<Server>
    {
        return std::make_shared<Server>(Config);
    }

    /**
     * @brief 使用指定工作线程数创建服务器。
     * @param Threads 工作线程数；0 表示使用硬件并发数。
     * @return 服务器共享指针。
     */
    [[nodiscard]] inline auto MakeServer(std::size_t Threads)
        -> std::shared_ptr<Server>
    {
        ServerConfig Config;
        Config.Threads = Threads;
        return std::make_shared<Server>(std::move(Config));
    }
}
