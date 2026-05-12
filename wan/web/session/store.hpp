/**
 * @file store.hpp
 * @brief Session 存储后端
 * @details Session 类定义和存储实现。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <boost/asio.hpp>
#include <chrono>
#include <memory>
#include <random>
#include <string>
#include <string_view>
#include <unordered_map>

namespace wan::web
{
    namespace net = boost::asio;

    /**
     * @class session
     * @brief Session 对象
     */
    class session
    {
    public:
        explicit session(std::string id)
            : id_(std::move(id))
            , created_at_(std::chrono::steady_clock::now())
            , modified_(false)
            , destroyed_(false)
        {
        }

        [[nodiscard]] auto id() const noexcept -> std::string_view
        {
            return id_;
        }

        [[nodiscard]] auto get(std::string_view key) const -> std::optional<std::string>
        {
            auto it = data_.find(std::string(key));
            if (it != data_.end())
            {
                return it->second;
            }
            return std::nullopt;
        }

        void set(std::string_view key, std::string_view value)
        {
            data_[std::string(key)] = std::string(value);
            modified_ = true;
        }

        void erase(std::string_view key)
        {
            data_.erase(std::string(key));
            modified_ = true;
        }

        [[nodiscard]] auto exists(std::string_view key) const -> bool
        {
            return data_.find(std::string(key)) != data_.end();
        }

        [[nodiscard]] auto data() const noexcept -> const std::unordered_map<std::string, std::string>&
        {
            return data_;
        }

        void destroy()
        {
            destroyed_ = true;
            data_.clear();
        }

        [[nodiscard]] auto destroyed() const noexcept -> bool
        {
            return destroyed_;
        }

        [[nodiscard]] auto modified() const noexcept -> bool
        {
            return modified_;
        }

        [[nodiscard]] auto created_at() const noexcept -> std::chrono::steady_clock::time_point
        {
            return created_at_;
        }

        void mark_saved()
        {
            modified_ = false;
        }

    private:
        std::string id_;
        std::unordered_map<std::string, std::string> data_;
        std::chrono::steady_clock::time_point created_at_;
        bool modified_;
        bool destroyed_;
    };

    using session_ptr = std::shared_ptr<session>;

    /**
     * @class session_store
     * @brief Session 存储接口
     */
    class session_store
    {
    public:
        virtual ~session_store() = default;

        virtual auto create() -> net::awaitable<session_ptr> = 0;
        virtual auto load(std::string_view id) -> net::awaitable<session_ptr> = 0;
        virtual auto save(session_ptr sess) -> net::awaitable<void> = 0;
        virtual auto destroy(std::string_view id) -> net::awaitable<void> = 0;
    };

    using session_store_ptr = std::shared_ptr<session_store>;

    /**
     * @class memory_store
     * @brief 内存 Session 存储
     */
    class memory_store : public session_store, public std::enable_shared_from_this<memory_store>
    {
    public:
        explicit memory_store(net::strand<net::any_io_executor> strand)
            : strand_(strand)
            , rng_(std::random_device{}())
        {
        }

        auto create() -> net::awaitable<session_ptr> override
        {
            auto self = shared_from_this();
            co_await net::post(strand_, net::use_awaitable);

            auto id = generate_id();
            auto sess = std::make_shared<session>(id);
            sessions_[id] = sess;
            co_return sess;
        }

        auto load(std::string_view id) -> net::awaitable<session_ptr> override
        {
            auto self = shared_from_this();
            co_await net::post(strand_, net::use_awaitable);

            auto it = sessions_.find(std::string(id));
            if (it != sessions_.end())
            {
                co_return it->second;
            }
            co_return nullptr;
        }

        auto save(session_ptr sess) -> net::awaitable<void> override
        {
            auto self = shared_from_this();
            co_await net::post(strand_, net::use_awaitable);
            sessions_[std::string(sess->id())] = sess;
        }

        auto destroy(std::string_view id) -> net::awaitable<void> override
        {
            auto self = shared_from_this();
            co_await net::post(strand_, net::use_awaitable);
            sessions_.erase(std::string(id));
        }

        auto cleanup(std::chrono::seconds max_age) -> net::awaitable<std::size_t>
        {
            auto self = shared_from_this();
            co_await net::post(strand_, net::use_awaitable);

            auto now = std::chrono::steady_clock::now();
            std::size_t removed = 0;

            for (auto it = sessions_.begin(); it != sessions_.end(); )
            {
                auto age = std::chrono::duration_cast<std::chrono::seconds>(now - it->second->created_at());
                if (age > max_age)
                {
                    it = sessions_.erase(it);
                    ++removed;
                }
                else
                {
                    ++it;
                }
            }
            co_return removed;
        }

    private:
        net::strand<net::any_io_executor> strand_;
        std::unordered_map<std::string, session_ptr> sessions_;
        std::mt19937 rng_;

        [[nodiscard]] auto generate_id() -> std::string
        {
            std::uniform_int_distribution<std::uint64_t> dist(0, UINT64_MAX);
            std::ostringstream oss;
            oss << std::hex << dist(rng_) << dist(rng_);
            return oss.str();
        }
    };

    [[nodiscard]] inline auto make_memory_store(net::strand<net::any_io_executor> strand) -> session_store_ptr
    {
        return std::make_shared<memory_store>(strand);
    }
}