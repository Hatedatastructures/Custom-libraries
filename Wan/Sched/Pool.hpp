/**
 * @file Pool.hpp
 * @brief 对 Boost.Asio 线程池的轻量封装。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include <boost/asio.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <thread>

namespace Wan::Sched
{
    namespace Net = boost::asio;

    /**
     * @class Pool
     * @brief 提交任务并管理线程池生命周期。
     */
    class Pool
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

    public:
        /**
         * @brief 创建线程池。
         * @param Threads 工作线程数；0 表示使用硬件并发数。
         */
        explicit Pool(std::size_t Threads = 0)
            : MemberPool(Threads > 0 ? Threads
                                : DefaultThreadCount())
        {
        }

        /** @brief 禁止复制构造。 */
        Pool(const Pool&) = delete;
        /** @brief 禁止复制赋值。 */
        auto operator=(const Pool&) -> Pool& = delete;

        /**
         * @brief 获取线程池的 Asio 执行器。
         * @return 关联此线程池的执行器。
         */
        [[nodiscard]] auto Executor() -> Net::any_io_executor
        {
            return MemberPool.get_executor();
        }

        /**
         * @brief 将任务提交到线程池。
         * @tparam FunctionType 可调用对象类型。
         * @param FunctionValue 要提交的任务。
         */
        template <typename FunctionType>
        void Post(FunctionType&& FunctionValue)
        {
            Net::post(MemberPool, std::forward<FunctionType>(FunctionValue));
        }

        /**
         * @brief 请求尽快停止线程池。
         * @note 尚未执行的排队任务可能不会运行。
         */
        void Stop()
        {
            MemberPool.stop();
        }

        /**
         * @brief 等待线程池结束。
         * @note 未先调用 `Stop()` 时，会等待待处理工作耗尽。
         */
        void Join()
        {
            MemberPool.join();
        }

        /**
         * @brief 获取底层线程池。
         * @return 底层线程池的可写引用，其生命周期受当前对象管理。
         */
        [[nodiscard]] auto NativePool() noexcept -> Net::thread_pool&
        {
            return MemberPool;
        }

    private:
        /** @brief 由当前包装器独占管理的 Asio 线程池。 */
        Net::thread_pool MemberPool;
    };
}
