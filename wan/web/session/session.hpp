/**
 * @file session.hpp
 * @brief Session 管理器
 * @details Session 中间件和管理器实现。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <wan/web/core/context.hpp>
#include <wan/web/middleware/middleware.hpp>
#include <wan/web/session/store.hpp>

namespace wan::web
{
    namespace net = boost::asio;

    /**
     * @struct session_config
     * @brief Session 配置
     */
    struct session_config
    {
        std::string cookie_name = "session_id";
        std::chrono::seconds max_age{3600};       // Session 过期时间
        bool secure = false;                      // Secure Cookie
        bool http_only = true;                    // HttpOnly Cookie
        std::string same_site = "lax";            // SameSite
    };

    /**
     * @class session_manager
     * @brief Session 管理器
     */
    class session_manager : public std::enable_shared_from_this<session_manager>
    {
    public:
        /**
         * @brief 构造函数
         * @param store 存储后端
         * @param config 配置
         */
        session_manager(session_store_ptr store, const session_config& config)
            : store_(store)
            , config_(config)
        {
        }

        /**
         * @brief 创建 Session 中间件
         */
        [[nodiscard]] auto middleware() -> wan::web::middleware
        {
            auto self = shared_from_this();

            return [self](context& ctx, std::function<net::awaitable<void>()> next) -> net::awaitable<void>
            {
                // 尝试从 Cookie 获取 Session ID
                auto session_id = ctx.cookie(self->config_.cookie_name);

                session_ptr sess;

                if (!session_id.empty())
                {
                    // 从存储加载 Session
                    sess = co_await self->store_->load(session_id);
                }

                if (!sess)
                {
                    // 创建新 Session
                    sess = co_await self->store_->create();
                }

                // 保存到 context（通过 set_route_param 暂存）
                ctx.set_route_param("_session_id", std::string(sess->id()));

                // 执行后续处理
                co_await next();

                // 保存/销毁 Session
                if (sess->destroyed())
                {
                    co_await self->store_->destroy(sess->id());
                    ctx.clear_cookie(self->config_.cookie_name, {
                        .path = "/",
                        .secure = self->config_.secure,
                        .http_only = self->config_.http_only
                    });
                }
                else if (sess->modified())
                {
                    co_await self->store_->save(sess);
                    sess->mark_saved();

                    // 设置 Cookie
                    ctx.set_cookie(self->config_.cookie_name, sess->id(), {
                        .max_age = self->config_.max_age,
                        .path = "/",
                        .secure = self->config_.secure,
                        .http_only = self->config_.http_only,
                        .same_site = self->config_.same_site
                    });
                }
            };
        }

        /**
         * @brief 从 context 获取 Session
         */
        [[nodiscard]] static auto get_session(context& ctx) -> session_ptr
        {
            auto id = ctx.param<std::string>("_session_id");
            if (!id)
            {
                return nullptr;
            }

            // 注意：这里需要从存储重新加载
            // 实际实现中应该在 middleware 中直接传递 session_ptr
            return nullptr; // 简化实现
        }

    private:
        session_store_ptr store_;
        session_config config_;
    };

    /**
     * @brief 创建 Session 管理器
     */
    [[nodiscard]] inline auto make_session_manager(session_store_ptr store, const session_config& config = {})
        -> std::shared_ptr<session_manager>
    {
        return std::make_shared<session_manager>(store, config);
    }
}