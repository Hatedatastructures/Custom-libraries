/**
 * @file Static.hpp
 * @brief 静态文件服务
 * @details 基于 Beast FileBody 的高效静态文件服务。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include <boost/beast.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

namespace Wan::Web
{
    namespace Http = boost::beast::http;

    /**
     * @class StaticFileServer
     * @brief 静态文件服务
     */
    class StaticFileServer
    {
    public:
        /**
         * @brief 创建静态文件服务。
         * @param Root 静态文件根目录。
         */
        explicit StaticFileServer(std::filesystem::path Root)
            : MemberRoot(std::move(Root))
        {
        }

        /**
         * @brief 根据路径扩展名获取 MIME 类型。
         * @param Path 文件路径或文件名。
         * @return 对应的 MIME 类型；未知扩展名返回 `application/octet-stream`。
         */
        [[nodiscard]] static auto MimeType(
            std::string_view Path) -> std::string
        {
            auto Dot = Path.rfind('.');
            if (Dot == std::string_view::npos)
            {
                return "application/octet-stream";
            }

            auto Ext = Path.substr(Dot + 1);
            static const std::unordered_map<std::string_view,
                std::string>
                Types = {
                    {"html", "text/html"},
                    {"htm", "text/html"},
                    {"css", "text/css"},
                    {"js", "application/javascript"},
                    {"json", "application/json"},
                    {"png", "image/png"},
                    {"jpg", "image/jpeg"},
                    {"jpeg", "image/jpeg"},
                    {"gif", "image/gif"},
                    {"svg", "image/svg+xml"},
                    {"ico", "image/x-icon"},
                    {"txt", "text/plain"},
                    {"xml", "application/xml"},
                    {"pdf", "application/pdf"},
                    {"zip", "application/zip"},
                    {"wasm", "application/wasm"},
                };

            auto It = Types.find(Ext);
            if (It != Types.end())
            {
                return It->second;
            }
            return "application/octet-stream";
        }

        /**
         * @brief 将请求目标解析为根目录内的文件路径。
         * @param Target 请求目标，可包含查询字符串。
         * @return 文件存在且位于根目录内时返回规范化路径，否则返回空值。
         */
        [[nodiscard]] auto Resolve(
            std::string_view Target) const
            -> std::optional<std::filesystem::path>
        {
            // 去掉查询参数
            auto Qpos = Target.find('?');
            auto PathStr = (Qpos != std::string_view::npos)
                ? Target.substr(0, Qpos)
                : Target;

            // 默认 index.html
            if (PathStr == "/")
            {
                PathStr = "/index.html";
            }

            auto FilePath =
                MemberRoot / std::filesystem::path{PathStr}.relative_path();

            // 规范化并检查路径穿越
            std::error_code FileEc;
            auto Canonical = std::filesystem::weakly_canonical(
                FilePath, FileEc);
            if (FileEc)
            {
                return std::nullopt;
            }
            auto RootCanonical = std::filesystem::weakly_canonical(
                MemberRoot, FileEc);
            if (FileEc)
            {
                return std::nullopt;
            }

            const auto Relative =
                Canonical.lexically_relative(RootCanonical);
            if (Relative.empty())
            {
                return std::nullopt;
            }

            for (const auto& Component : Relative)
            {
                if (Component == "..")
                {
                    return std::nullopt;
                }
            }

            if (!std::filesystem::exists(Canonical, FileEc)
                || FileEc
                || !std::filesystem::is_regular_file(Canonical, FileEc)
                || FileEc)
            {
                return std::nullopt;
            }

            return Canonical;
        }

        /**
         * @brief 获取静态文件根目录。
         * @return 根目录路径的只读引用。
         */
        [[nodiscard]] auto Root() const noexcept
            -> const std::filesystem::path&
        {
            return MemberRoot;
        }

    private:
        /** @brief 静态文件查找与路径限制所用的根目录。 */
        std::filesystem::path MemberRoot;
    };
}
