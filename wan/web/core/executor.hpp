/**
 * @file executor.hpp
 * @brief 协程执行器
 * @details 提供 Web 模块的协程运行环境，使用 Strand 保证线程安全。
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

#include <boost/asio.hpp>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

namespace wan::web
{
    namespace net = boost::asio;

    /**
     * @class executor
     * @brief 协程执行器
     * @details 管理 io_context 和线程池，提供 Strand 保护的协程运行环境。
     * 支持零配置启动（自动检测 CPU 核心数）或指定线程数。
     */
    class executor
    {
    public:
        /**
         * @brief 构造函数（零配置）
         * @details 自动检测 CPU 核心数作为线程数。
         */
        executor()
            : threads_(std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4)
            , ioc_(threads_)
            , serial_exec_(net::make_strand(ioc_))
        {
        }

        /**
         * @brief 构造函数（指定线程数）
         * @param threads 线程数
         */
        explicit executor(std::size_t threads)
            : threads_(threads > 0 ? threads : 1)
            , ioc_(threads_)
            , serial_exec_(net::make_strand(ioc_))
        {
        }

        /**
         * @brief 析构函数
         * @details 停止执行器并等待所有线程结束。
         */
        ~executor()
        {
            stop();
        }

        /**
         * @brief 获取 Strand
         * @return Strand 执行器，用于串行化异步操作
         */
        [[nodiscard]] auto strand() noexcept -> net::strand<net::any_io_executor>
        {
            return serial_exec_;
        }

        /**
         * @brief 获取底层 io_context
         * @return io_context 引用
         */
        [[nodiscard]] auto context() noexcept -> net::io_context&
        {
            return ioc_;
        }

        /**
         * @brief 启动执行器
         * @details 启动线程池运行 io_context。
         */
        void start()
        {
            if (running_)
            {
                return;
            }
            running_ = true;

            workers_.reserve(threads_);
            for (std::size_t i = 0; i < threads_; ++i)
            {
                workers_.emplace_back([this]
                {
                    ioc_.run();
                });
            }
        }

        /**
         * @brief 停止执行器
         * @details 停止 io_context 并等待所有线程结束。
         */
        void stop()
        {
            if (!running_)
            {
                return;
            }
            running_ = false;

            ioc_.stop();

            for (auto& w : workers_)
            {
                if (w.joinable())
                {
                    w.join();
                }
            }
            workers_.clear();

            // 重置 io_context 以便重新启动
            ioc_.restart();
        }

        /**
         * @brief 运行协程
         * @details 在 Strand 中调度协程执行。
         * @tparam Func 协程函数类型
         * @param func 协程函数
         */
        template <typename Func>
        void spawn(Func&& func)
        {
            net::co_spawn(serial_exec_, std::forward<Func>(func), net::detached);
        }

        /**
         * @brief 在 Strand 上等待调度
         * @details 协程中使用，确保后续操作在 Strand 中执行。
         */
        auto dispatch() -> net::awaitable<void>
        {
            co_await net::dispatch(serial_exec_, net::use_awaitable);
        }

    private:
        std::size_t threads_;
        net::io_context ioc_;
        net::strand<net::any_io_executor> serial_exec_;
        std::vector<std::thread> workers_;
        bool running_ = false;
    };

    /**
     * @brief 创建执行器
     * @param threads 线程数（默认零配置）
     * @return 执行器共享指针
     */
    [[nodiscard]] inline auto make_executor(std::size_t threads = 0) -> std::shared_ptr<executor>
    {
        if (threads > 0)
        {
            return std::make_shared<executor>(threads);
        }
        return std::make_shared<executor>();
    }
}