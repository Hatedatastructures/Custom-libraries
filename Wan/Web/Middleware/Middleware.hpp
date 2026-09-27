/**
 * @file Middleware.hpp
 * @brief 中间件类型系统
 * @details 洋葱模型中间件，支持优先级排序和类型擦除。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include <boost/asio.hpp>

#include <functional>
#include <string>
#include <vector>

#include "Wan/Web/Core/Context.hpp"
#include "Wan/Web/Core/Handler.hpp"

namespace Wan::Web
{
    namespace Net = boost::asio;

    /**
     * @brief 继续执行中间件链的异步函数类型。
     */
    using NextFn = std::function<Net::awaitable<void>()>;

    /**
     * @brief 接收请求上下文和继续函数的异步中间件类型。
     */
    using MiddlewareFn =
        std::function<Net::awaitable<void>(Context&, NextFn)>;

    /**
     * @struct Middleware
     * @brief 带优先级的中间件
     */
    struct Middleware
    {
        /** @brief 中间件函数。 */
        MiddlewareFn Fn;
        /** @brief 执行优先级；数值较小的中间件先运行。 */
        int Priority = 0;
        /** @brief 用于识别中间件的名称。 */
        std::string Name;

        /**
         * @brief 按优先级比较中间件。
         * @param Other 另一个中间件。
         * @return 当前优先级较小时为 `true`。
         */
        auto operator<(const Middleware& Other) const -> bool
        {
            return Priority < Other.Priority;
        }
    };

    /**
     * @class MiddlewareChain
     * @brief 按优先级顺序执行中间件。
     */
    class MiddlewareChain
    {
    public:
        /**
         * @brief 添加中间件。
         * @param M 要添加的中间件。
         */
        void Use(Middleware M)
        {
            MemberMiddlewares.push_back(std::move(M));
            std::stable_sort(MemberMiddlewares.begin(), MemberMiddlewares.end());
        }

        /**
         * @brief 添加中间件函数。
         * @param Fn 中间件函数。
         * @param Priority 执行优先级；数值较小的中间件先运行。
         * @param Name 中间件名称。
         */
        void Use(MiddlewareFn Fn, int Priority = 0,
            std::string Name = "")
        {
            Use(Middleware{std::move(Fn), Priority, std::move(Name)});
        }

        /**
         * @brief 运行中间件链并在末尾调用最终处理器。
         * @param Ctx 请求上下文。
         * @param FinalHandler 最终请求处理器。
         * @return 表示链执行过程的 Asio awaitable。
         */
        [[nodiscard]] auto Run(
            Context& Ctx, Handler FinalHandler) -> Net::awaitable<void>
        {
            if (MemberMiddlewares.empty())
            {
                co_await FinalHandler(Ctx);
                co_return;
            }

            std::size_t Index = 0;
            NextFn Next = [&]() -> Net::awaitable<void>
            {
                if (Index < MemberMiddlewares.size())
                {
                    auto& Mw = MemberMiddlewares[Index++];
                    co_await Mw.Fn(Ctx, Next);
                }
                else
                {
                    co_await FinalHandler(Ctx);
                }
            };

            co_await Next();
        }

        /**
         * @brief 检查链中是否没有中间件。
         * @return 没有中间件时为 `true`。
         */
        [[nodiscard]] auto Empty() const noexcept -> bool
        {
            return MemberMiddlewares.empty();
        }

        /**
         * @brief 获取中间件数量。
         * @return 当前中间件数。
         */
        [[nodiscard]] auto Size() const noexcept -> std::size_t
        {
            return MemberMiddlewares.size();
        }

    private:
        /** @brief 按执行顺序保存的中间件列表。 */
        std::vector<Middleware> MemberMiddlewares;
    };
}
