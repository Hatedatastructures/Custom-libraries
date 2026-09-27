/**
 * @file Executor.hpp
 * @brief Web 执行器
 * @details 使用一个 `io_context` 驱动工作线程，并负责记录工作处理器异常。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include <boost/asio.hpp>

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace Wan::Web
{
    namespace Net = boost::asio;

    /**
     * @class Executor
     * @brief Web 服务执行器
     */
    class Executor
    {
        /**
         * @brief 返回默认工作线程数。
         * @return 硬件并发数；无法探测时至少返回一个线程。
         */
        [[nodiscard]] static auto DefaultThreadCount() noexcept
            -> std::size_t
        {
            return std::max<std::size_t>(
                1, std::thread::hardware_concurrency());
        }

        /**
         * @brief 解析并校验实际工作线程数。
         * @param Threads 请求的线程数；零表示使用默认线程数。
         * @return 可传给 Asio 的线程数。
         * @throws std::invalid_argument 线程数超出 Asio 的整数范围时抛出。
         */
        [[nodiscard]] static auto ResolveThreadCount(
            std::size_t Threads) -> std::size_t
        {
            const auto EffectiveThreads = Threads > 0
                ? Threads
                : DefaultThreadCount();
            if (EffectiveThreads > static_cast<std::size_t>(
                    std::numeric_limits<int>::max()))
            {
                throw std::invalid_argument(
                    "executor thread count exceeds the Asio limit");
            }
            return EffectiveThreads;
        }

    public:
        /**
         * @brief 构造执行器
         * @param Threads 工作线程数；0 表示使用硬件并发数
         * @throws std::invalid_argument 线程数超出 Asio 支持范围时
         */
        explicit Executor(std::size_t Threads = 0)
            : MemberIoc{static_cast<int>(ResolveThreadCount(Threads))}
            , MemberWorkGuard(MemberIoc.get_executor())
            , MemberThreads(ResolveThreadCount(Threads))
        {
        }

        ~Executor() noexcept
        {
            try
            {
                Stop();
                Join();
            }
            catch (...)
            {
                std::terminate();
            }
        }

        /**
         * @brief 启动工作线程
         * @throws std::logic_error 执行器已启动或已停止时
         * @throws std::system_error 线程创建失败时；已创建线程会先停止并 Join
         */
        void Run()
        {
            {
                std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                if (MemberState != Lifecycle::Created)
                {
                    throw std::logic_error(
                        "executor can only be started once");
                }
                MemberState = Lifecycle::Starting;
            }

            try
            {
                MemberWorkers.reserve(MemberThreads);
                for (std::size_t I = 0; I < MemberThreads; ++I)
                {
                    MemberWorkers.emplace_back([this]()
                    {
                        try
                        {
                            MemberIoc.run();
                        }
                        catch (...)
                        {
                            RecordWorkerFailure(std::current_exception());
                        }
                    });
                }
            }
            catch (...)
            {
                const auto LaunchFailure = std::current_exception();
                Stop();
                for (auto& Worker : MemberWorkers)
                {
                    if (Worker.joinable())
                    {
                        Worker.join();
                    }
                }
                {
                    std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                    MemberState = Lifecycle::Joined;
                }
                MemberLifecycleChanged.notify_all();
                std::rethrow_exception(LaunchFailure);
            }

            {
                std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                MemberState = MemberStopRequested
                    ? Lifecycle::Stopping : Lifecycle::Running;
            }
            MemberLifecycleChanged.notify_all();
        }

        /**
         * @brief 等待所有工作线程结束
         * @throws std::logic_error 从工作线程自身调用或另一个 Join 正在进行时
         * @note 工作处理器抛出的异常会在此处原样重抛一次。
         */
        void Join()
        {
            std::vector<std::thread> Workers;
            {
                std::unique_lock<std::mutex> Lock{MemberLifecycleMutex};
                MemberLifecycleChanged.wait(Lock, [this]
                {
                    return MemberState != Lifecycle::Starting;
                });
                if (MemberState == Lifecycle::Joined)
                {
                    Lock.unlock();
                    RethrowWorkerFailure();
                    return;
                }
                if (MemberState == Lifecycle::Joining)
                {
                    throw std::logic_error(
                        "executor Join is already in progress");
                }
                for (const auto& Worker : MemberWorkers)
                {
                    if (Worker.joinable()
                        && Worker.get_id() == std::this_thread::get_id())
                    {
                        throw std::logic_error(
                            "executor worker cannot join itself");
                    }
                }
                MemberState = Lifecycle::Joining;
                Workers.swap(MemberWorkers);
            }

            for (auto& Worker : Workers)
            {
                if (Worker.joinable())
                {
                    Worker.join();
                }
            }
            {
                std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                MemberState = Lifecycle::Joined;
            }
            MemberLifecycleChanged.notify_all();
            RethrowWorkerFailure();
        }

        /**
         * @brief 停止 IoContext
         * @details 幂等；不等待工作线程退出。
         */
        void Stop()
        {
            {
                std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                if (MemberState == Lifecycle::Created)
                {
                    MemberState = Lifecycle::Stopping;
                }
                else if (MemberState == Lifecycle::Starting)
                {
                    MemberStopRequested = true;
                }
                else if (MemberState == Lifecycle::Running)
                {
                    MemberState = Lifecycle::Stopping;
                }
                if (MemberState == Lifecycle::Joined)
                {
                    return;
                }
                MemberWorkGuard.reset();
                MemberIoc.stop();
            }
            MemberLifecycleChanged.notify_all();
        }

        /**
         * @brief 在线程 Join 后重新运行上下文，处理排队的取消回调。
         * @throws std::logic_error 尚未 Join 工作线程时抛出。
         * @note 回调抛出的异常会在此处原样重抛一次。
         */
        void DrainStopped()
        {
            {
                std::lock_guard<std::mutex> Lock{MemberLifecycleMutex};
                if (MemberState != Lifecycle::Joined)
                {
                    throw std::logic_error(
                        "executor can only drain after Join");
                }
            }

            MemberWorkGuard.reset();
            MemberIoc.stop();
            MemberIoc.restart();
            for (;;)
            {
                try
                {
                    MemberIoc.run();
                    break;
                }
                catch (...)
                {
                    RecordWorkerFailure(std::current_exception());
                }
            }
            RethrowWorkerFailure();
        }

        /**
         * @brief 获取执行器
         * @return 关联的 `io_context` 执行器。
         */
        [[nodiscard]] auto GetExecutor() -> Net::any_io_executor
        {
            return MemberIoc.get_executor();
        }

        /**
         * @brief 获取 IoContext
         * @return 此执行器持有的 `io_context` 引用。
         */
        [[nodiscard]] auto Ioc() noexcept -> Net::io_context&
        {
            return MemberIoc;
        }

        /**
         * @brief 在执行器上启动协程
         * @tparam F Asio awaitable 类型，或返回 awaitable 的可调用对象类型。
         * @param Fn 要启动的协程
         */
        template <typename F>
        void Spawn(F&& Fn)
        {
            Spawn(Net::any_io_executor{MemberIoc.get_executor()},
                std::forward<F>(Fn));
        }

        /**
         * @brief 在指定执行器上启动协程。
         * @tparam F Asio awaitable 类型，或返回 awaitable 的可调用对象类型。
         * @param ExecutorValue 必须关联到此对象持有的 `io_context`。
         * @param Fn 要启动的协程。
         * @throws std::invalid_argument 执行器关联到其他上下文时抛出。
         */
        template <typename F>
        void Spawn(Net::any_io_executor ExecutorValue, F&& Fn)
        {
            if (&ExecutorValue.context()
                != static_cast<Net::execution_context*>(&MemberIoc))
            {
                throw std::invalid_argument(
                    "Executor::Spawn requires an executor from its own io_context");
            }
            Net::co_spawn(ExecutorValue,
                std::forward<F>(Fn), [this](std::exception_ptr Failure)
                {
                    if (Failure)
                    {
                        RecordWorkerFailure(Failure);
                    }
                });
        }

    private:
        /** @brief 执行器的生命周期状态。 */
        enum class Lifecycle
        {
            Created, ///< 尚未启动。
            Starting, ///< 正在创建 worker 线程。
            Running, ///< 正在运行。
            Stopping, ///< 已请求停止。
            Joining, ///< 正在等待 worker 线程退出。
            Joined, ///< 所有 worker 线程已退出。
        };

        /** @brief 取出并重新抛出已记录的 worker 异常。 */
        void RethrowWorkerFailure()
        {
            std::exception_ptr Failure;
            {
                std::lock_guard<std::mutex> Lock{MemberFailureMutex};
                Failure = MemberWorkerFailure;
                MemberWorkerFailure = nullptr;
            }
            if (Failure)
            {
                std::rethrow_exception(Failure);
            }
        }

        /**
         * @brief 记录首个 worker 异常并请求停止执行器。
         * @param Failure worker 捕获的异常。
         */
        void RecordWorkerFailure(std::exception_ptr Failure)
        {
            {
                std::lock_guard<std::mutex> Lock{MemberFailureMutex};
                if (!MemberWorkerFailure)
                {
                    MemberWorkerFailure = std::move(Failure);
                }
            }
            Stop();
        }

        /** @brief 执行异步任务的 Asio io_context。 */
        Net::io_context MemberIoc;
        /** @brief 在执行器关闭前保持 io_context 处于运行状态的 work guard。 */
        Net::executor_work_guard<Net::io_context::executor_type>
            MemberWorkGuard;
        /** @brief 启动时使用的 worker 线程数。 */
        std::size_t MemberThreads;
        /** @brief 执行 io_context::run 的 worker 线程。 */
        std::vector<std::thread> MemberWorkers;
        /** @brief 保护生命周期状态的互斥量。 */
        std::mutex MemberLifecycleMutex;
        /** @brief 通知生命周期等待方状态变化的条件变量。 */
        std::condition_variable MemberLifecycleChanged;
        /** @brief 当前生命周期状态。 */
        Lifecycle MemberState = Lifecycle::Created;
        /** @brief 是否已请求停止。 */
        bool MemberStopRequested = false;
        /** @brief 保护首个 worker 异常的互斥量。 */
        std::mutex MemberFailureMutex;
        /** @brief 首个 worker 异常；尚未发生异常时为空。 */
        std::exception_ptr MemberWorkerFailure;
    };
}
