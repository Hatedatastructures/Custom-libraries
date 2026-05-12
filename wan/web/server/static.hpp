/**
 * @file static.hpp
 * @brief 静态文件服务
 * @details 高性能静态文件服务，支持 Range 请求、ETag、MIME 检测。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#include <wan/web/core/context.hpp>
#include <wan/web/middleware/middleware.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace fs = std::filesystem;

    /**
     * @class static_server
     * @brief 静态文件服务器
     * @details 提供 MIME 检测、Range 请求、ETag 缓存。
     */
    class static_server
    {
    public:
        /**
         * @brief 构造函数
         * @param root 根目录路径
         */
        explicit static_server(std::string_view root)
            : root_(root)
        {
            init_mime_types();
        }

        /**
         * @brief 创建静态文件中间件
         * @return 中间件
         */
        [[nodiscard]] auto serve() -> middleware
        {
            auto self = std::make_shared<static_server>(root_);

            return [self](context& ctx, std::function<net::awaitable<void>()> next) -> net::awaitable<void>
            {
                // 只处理 GET 和 HEAD
                if (ctx.method() != http::verb::get && ctx.method() != http::verb::head)
                {
                    co_await next();
                    co_return;
                }

                auto target = ctx.target();

                // 解析查询参数
                auto query_pos = target.find('?');
                if (query_pos != std::string_view::npos)
                {
                    target = target.substr(0, query_pos);
                }

                // 构造文件路径
                auto file_path = self->resolve_path(target);

                // 检查路径安全
                if (!self->is_safe_path(file_path))
                {
                    ctx.status(http::status::forbidden).text("Forbidden");
                    co_return;
                }

                // 检查文件存在
                if (!fs::exists(file_path) || !fs::is_regular_file(file_path))
                {
                    co_await next();
                    co_return;
                }

                // 处理文件请求
                co_await self->serve_file(ctx, file_path);
            };
        }

    private:
        std::string root_;
        std::unordered_map<std::string, std::string> mime_types_;

        /**
         * @brief 初始化 MIME 类型映射
         */
        void init_mime_types()
        {
            mime_types_ = {
                {"html", "text/html"},
                {"htm", "text/html"},
                {"css", "text/css"},
                {"js", "application/javascript"},
                {"json", "application/json"},
                {"xml", "application/xml"},
                {"png", "image/png"},
                {"jpg", "image/jpeg"},
                {"jpeg", "image/jpeg"},
                {"gif", "image/gif"},
                {"svg", "image/svg+xml"},
                {"ico", "image/x-icon"},
                {"webp", "image/webp"},
                {"woff", "font/woff"},
                {"woff2", "font/woff2"},
                {"ttf", "font/ttf"},
                {"pdf", "application/pdf"},
                {"zip", "application/zip"},
                {"txt", "text/plain"},
                {"md", "text/markdown"},
                {"mp4", "video/mp4"},
                {"mp3", "audio/mpeg"},
                {"wav", "audio/wav"}
            };
        }

        /**
         * @brief 解析文件路径
         */
        [[nodiscard]] auto resolve_path(std::string_view target) const -> fs::path
        {
            std::string path(target);

            // 处理 URL 编码（简化版）
            // 实际应该完整解码 URL

            // 去掉前导 '/'
            while (!path.empty() && path.front() == '/')
            {
                path = path.substr(1);
            }

            // 如果路径为空，默认 index.html
            if (path.empty())
            {
                path = "index.html";
            }

            return fs::path(root_) / path;
        }

        /**
         * @brief 检查路径安全（防止目录遍历）
         */
        [[nodiscard]] auto is_safe_path(const fs::path& path) const -> bool
        {
            // 规范化路径
            auto canonical = fs::weakly_canonical(path);
            auto root_canonical = fs::weakly_canonical(fs::path(root_));

            // 检查是否在根目录内
            auto rel = fs::relative(canonical, root_canonical);

            // 如果相对路径以 .. 开头，说明在根目录外
            auto rel_str = rel.string();
            if (rel.empty() || rel_str.find("..") != std::string::npos)
            {
                return false;
            }

            return true;
        }

        /**
         * @brief 获取 MIME 类型
         */
        [[nodiscard]] auto get_mime_type(const fs::path& path) const -> std::string
        {
            auto ext = path.extension().string();
            if (!ext.empty())
            {
                ext = ext.substr(1); // 去掉 '.'
            }

            auto it = mime_types_.find(ext);
            if (it != mime_types_.end())
            {
                return it->second;
            }

            return "application/octet-stream";
        }

        /**
         * @brief 生成 ETag
         */
        [[nodiscard]] auto generate_etag(const fs::path& path) const -> std::string
        {
            // ETag = 文件大小 + 修改时间
            auto size = fs::file_size(path);
            auto mtime = fs::last_write_time(path);

            std::ostringstream oss;
            oss << "\"" << size << "-" << mtime.time_since_epoch().count() << "\"";
            return oss.str();
        }

        /**
         * @brief 服务文件
         */
        auto serve_file(context& ctx, const fs::path& path) -> net::awaitable<void>
        {
            auto self = std::shared_ptr<static_server>(this, [](auto*){}); // 不管理生命周期

            auto mime = get_mime_type(path);
            auto etag = generate_etag(path);
            auto file_size = fs::file_size(path);

            // 检查 If-None-Match（缓存命中）
            auto if_none_match = ctx.header("If-None-Match");
            if (!if_none_match.empty() && if_none_match == etag)
            {
                ctx.status(http::status::not_modified);
                ctx.set("ETag", etag);
                ctx.set("Cache-Control", "public, max-age=3600");
                co_return;
            }

            // 检查 Range 请求（断点续传）
            auto range_header = ctx.header("Range");
            bool is_range_request = !range_header.empty() && range_header.starts_with("bytes=");

            if (is_range_request)
            {
                // 解析 Range: bytes=start-end
                auto range_spec = range_header.substr(6); // "bytes=" 后面

                std::uint64_t start = 0;
                std::uint64_t end = file_size - 1;

                auto dash_pos = range_spec.find('-');
                if (dash_pos != std::string_view::npos)
                {
                    auto start_str = range_spec.substr(0, dash_pos);
                    auto end_str = range_spec.substr(dash_pos + 1);

                    if (!start_str.empty())
                    {
                        start = std::stoull(std::string(start_str));
                    }

                    if (!end_str.empty())
                    {
                        end = std::stoull(std::string(end_str));
                    }
                }

                // 验证范围
                if (start >= file_size || end >= file_size || start > end)
                {
                    ctx.status(http::status::range_not_satisfiable);
                    ctx.set("Content-Range", "bytes */" + std::to_string(file_size));
                    co_return;
                }

                // 读取部分文件
                std::ifstream file(path, std::ios::binary);
                if (!file)
                {
                    ctx.status(http::status::internal_server_error).text("Failed to open file");
                    co_return;
                }

                file.seekg(static_cast<std::streamoff>(start));
                auto range_size = end - start + 1;
                std::string content(range_size, '\0');
                file.read(content.data(), static_cast<std::streamsize>(range_size));

                ctx.status(http::status::partial_content);
                ctx.set("Content-Type", mime);
                ctx.set("Content-Range", "bytes " + std::to_string(start) + "-" + std::to_string(end) + "/" + std::to_string(file_size));
                ctx.set("Content-Length", std::to_string(range_size));
                ctx.set("ETag", etag);
                ctx.set("Cache-Control", "public, max-age=3600");
                ctx.set("Accept-Ranges", "bytes");

                // HEAD 请求只返回头部
                if (ctx.method() == http::verb::head)
                {
                    co_return;
                }

                ctx.raw_response().body() = std::move(content);
            }
            else
            {
                // 完整文件
                std::ifstream file(path, std::ios::binary);
                if (!file)
                {
                    ctx.status(http::status::internal_server_error).text("Failed to open file");
                    co_return;
                }

                std::string content(file_size, '\0');
                file.read(content.data(), static_cast<std::streamsize>(file_size));

                ctx.status(http::status::ok);
                ctx.set("Content-Type", mime);
                ctx.set("Content-Length", std::to_string(file_size));
                ctx.set("ETag", etag);
                ctx.set("Cache-Control", "public, max-age=3600");
                ctx.set("Accept-Ranges", "bytes");

                // HEAD 请求只返回头部
                if (ctx.method() == http::verb::head)
                {
                    co_return;
                }

                ctx.raw_response().body() = std::move(content);
            }
        }
    };

    /**
     * @brief 创建静态文件服务
     */
    [[nodiscard]] inline auto make_static(std::string_view root) -> middleware
    {
        return static_server(root).serve();
    }
}