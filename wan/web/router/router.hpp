/**
 * @file router.hpp
 * @brief Radix Tree 路由器
 * @details 高性能路由匹配，使用 Radix Tree 实现 O(K) 复杂度。
 * @author Hatedatastructures
 * @date 2026-05-12
 */
#pragma once

#include <boost/asio.hpp>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <wan/web/core/context.hpp>

namespace wan::web
{
    namespace net = boost::asio;
    namespace http = boost::beast::http;

    /**
     * @brief 处理器类型
     */
    using handler = std::function<net::awaitable<void>(context&)>;

    /**
     * @struct route_match
     * @brief 路由匹配结果
     */
    struct route_match
    {
        handler handler;
        std::map<std::string, std::string> params;
    };

    /**
     * @enum param_type
     * @brief 参数类型
     */
    enum class param_type : uint8_t
    {
        none = 0,    // 无参数
        int_param,   // <int:id>
        string_param,// <string:name>
        path_param   // <path:filename>
    };

    /**
     * @class radix_node
     * @brief Radix Tree 节点
     */
    class radix_node : public std::enable_shared_from_this<radix_node>
    {
    public:
        std::string prefix;                         // 节点前缀
        std::vector<std::shared_ptr<radix_node>> children; // 子节点
        handler handler;                            // 处理器（叶子节点）
        param_type param_type = param_type::none;   // 参数类型
        std::string param_name;                     // 参数名
        bool is_wildcard = false;                   // 是否是通配节点

        /**
         * @brief 检查是否是叶子节点（有处理器）
         */
        [[nodiscard]] auto is_leaf() const noexcept -> bool
        {
            return static_cast<bool>(handler);
        }

        /**
         * @brief 检查是否是参数节点
         */
        [[nodiscard]] auto is_param() const noexcept -> bool
        {
            return param_type != param_type::none;
        }
    };

    /**
     * @class radix_tree
     * @brief Radix Tree
     */
    class radix_tree
    {
    public:
        radix_tree()
            : root_(std::make_shared<radix_node>())
        {
        }

        /**
         * @brief 插入路由
         * @param path 路径
         * @param h 处理器
         */
        void insert(std::string_view path, handler h)
        {
            auto node = root_;
            std::size_t i = 0;

            while (i < path.size())
            {
                // 检查是否是参数
                if (path[i] == '<')
                {
                    // 解析参数：<type:name>
                    auto end = path.find('>', i);
                    if (end == std::string_view::npos)
                    {
                        break;
                    }

                    auto param_spec = path.substr(i + 1, end - i - 1);
                    auto colon_pos = param_spec.find(':');

                    std::string type_str;
                    std::string name;

                    if (colon_pos != std::string_view::npos)
                    {
                        type_str = std::string(param_spec.substr(0, colon_pos));
                        name = std::string(param_spec.substr(colon_pos + 1));
                    }
                    else
                    {
                        type_str = "string";
                        name = std::string(param_spec);
                    }

                    // 创建参数节点
                    auto param_node = std::make_shared<radix_node>();
                    param_node->param_name = name;

                    if (type_str == "int")
                    {
                        param_node->param_type = param_type::int_param;
                    }
                    else if (type_str == "path")
                    {
                        param_node->param_type = param_type::path_param;
                        param_node->is_wildcard = true;
                    }
                    else
                    {
                        param_node->param_type = param_type::string_param;
                    }

                    // 查找或创建子节点
                    bool found = false;
                    for (auto& child : node->children)
                    {
                        if (child->is_param())
                        {
                            found = true;
                            node = child;
                            break;
                        }
                    }

                    if (!found)
                    {
                        node->children.push_back(param_node);
                        node = param_node;
                    }

                    i = end + 1;
                }
                else
                {
                    // 查找匹配的静态子节点
                    auto matched = find_static_child(node, path, i);

                    if (matched.child)
                    {
                        // 部分匹配，继续
                        i += matched.matched_len;
                        node = matched.child;
                    }
                    else
                    {
                        // 无匹配，创建新节点
                        auto new_node = std::make_shared<radix_node>();
                        new_node->prefix = std::string(path.substr(i));
                        node->children.push_back(new_node);
                        node = new_node;
                        break;
                    }
                }
            }

            // 设置处理器
            node->handler = std::move(h);
        }

        /**
         * @brief 查找路由
         * @param path 路径
         * @return 匹配结果
         */
        [[nodiscard]] auto search(std::string_view path) const -> std::optional<route_match>
        {
            std::map<std::string, std::string> params;
            auto result = search_node(root_, path, 0, params);

            if (result)
            {
                route_match match;
                match.handler = result->get()->handler;
                match.params = std::move(params);
                return match;
            }

            return std::nullopt;
        }

    private:
        struct match_result
        {
            std::shared_ptr<radix_node> child;
            std::size_t matched_len = 0;
        };

        /**
         * @brief 查找静态子节点
         */
        [[nodiscard]] auto find_static_child(const std::shared_ptr<radix_node>& node, std::string_view path, std::size_t start) const
            -> match_result
        {
            for (auto& child : node->children)
            {
                if (child->is_param())
                {
                    continue;
                }

                // 计算公共前缀长度
                std::size_t common_len = 0;
                auto child_len = child->prefix.size();
                auto remaining = path.size() - start;

                while (common_len < child_len && common_len < remaining)
                {
                    if (child->prefix[common_len] != path[start + common_len])
                    {
                        break;
                    }
                    ++common_len;
                }

                if (common_len > 0)
                {
                    return {child, common_len};
                }
            }

            return {nullptr, 0};
        }

        /**
         * @brief 递归搜索节点
         */
        [[nodiscard]] auto search_node(const std::shared_ptr<radix_node>& node, std::string_view path,
                                        std::size_t pos, std::map<std::string, std::string>& params) const
            -> std::optional<std::shared_ptr<radix_node>>
        {
            if (pos >= path.size())
            {
                // 到达路径末尾
                if (node->is_leaf())
                {
                    return node;
                }
                return std::nullopt;
            }

            // 跳过 '/'
            if (path[pos] == '/')
            {
                ++pos;
                if (pos >= path.size())
                {
                    if (node->is_leaf())
                    {
                        return node;
                    }
                    return std::nullopt;
                }
            }

            // 优先匹配静态节点
            for (auto& child : node->children)
            {
                if (child->is_param())
                {
                    continue;
                }

                // 检查前缀匹配
                if (path.substr(pos).starts_with(child->prefix))
                {
                    auto new_pos = pos + child->prefix.size();
                    auto result = search_node(child, path, new_pos, params);
                    if (result)
                    {
                        return result;
                    }
                }
            }

            // 尝试参数节点
            for (auto& child : node->children)
            {
                if (!child->is_param())
                {
                    continue;
                }

                // 提取参数值
                std::string param_value;
                std::size_t new_pos = pos;

                if (child->param_type == param_type::path_param)
                {
                    // path 参数：匹配剩余所有路径
                    param_value = std::string(path.substr(pos));
                    new_pos = path.size();
                }
                else
                {
                    // int/string 参数：匹配到下一个 '/'
                    while (new_pos < path.size() && path[new_pos] != '/')
                    {
                        // int 参数需要验证数字
                        if (child->param_type == param_type::int_param)
                        {
                            if (!std::isdigit(static_cast<unsigned char>(path[new_pos])))
                            {
                                break;
                            }
                        }
                        param_value += path[new_pos];
                        ++new_pos;
                    }

                    if (param_value.empty())
                    {
                        continue;
                    }
                }

                // 递归搜索
                params[child->param_name] = param_value;
                auto result = search_node(child, path, new_pos, params);
                if (result)
                {
                    return result;
                }

                // 回溯
                params.erase(child->param_name);
            }

            return std::nullopt;
        }

        std::shared_ptr<radix_node> root_;
    };

    /**
     * @class router
     * @brief 路由器
     * @details 使用 Radix Tree 实现高性能路由匹配。
     */
    class router
    {
    public:
        /**
         * @brief 注册 GET 路由
         */
        auto get(std::string_view pattern, handler h) -> router&
        {
            add_route(http::verb::get, pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 POST 路由
         */
        auto post(std::string_view pattern, handler h) -> router&
        {
            add_route(http::verb::post, pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 PUT 路由
         */
        auto put(std::string_view pattern, handler h) -> router&
        {
            add_route(http::verb::put, pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 DELETE 路由
         */
        auto del(std::string_view pattern, handler h) -> router&
        {
            add_route(http::verb::delete_, pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 PATCH 路由
         */
        auto patch(std::string_view pattern, handler h) -> router&
        {
            add_route(http::verb::patch, pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 HEAD 路由
         */
        auto head(std::string_view pattern, handler h) -> router&
        {
            add_route(http::verb::head, pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册 OPTIONS 路由
         */
        auto options(std::string_view pattern, handler h) -> router&
        {
            add_route(http::verb::options, pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 注册路由组（共享前缀）
         */
        auto group(std::string_view prefix, std::function<void(router&)> setup) -> router&
        {
            // 创建临时路由器
            router sub_router;
            setup(sub_router);

            // 将子路由器合并到当前路由器（添加前缀）
            for (auto& [method, tree] : sub_router.trees_)
            {
                // 重新插入带有前缀的路由
                // 简化实现：直接添加到当前路由器
                auto& current_tree = trees_[method];
                // TODO: 需要更复杂的实现来复制树
            }

            return *this;
        }

        /**
         * @brief 注册通用路由
         */
        auto route(http::verb method, std::string_view pattern, handler h) -> router&
        {
            add_route(method, pattern, std::move(h));
            return *this;
        }

        /**
         * @brief 匹配路由（O(K) 复杂度）
         * @param method HTTP 方法
         * @param target 请求路径
         * @return 匹配结果
         */
        [[nodiscard]] auto match(http::verb method, std::string_view target) const -> std::optional<route_match>
        {
            // 解析查询参数
            std::string path(target);
            auto query_pos = path.find('?');
            if (query_pos != std::string::npos)
            {
                path = path.substr(0, query_pos);
            }

            // 查找方法树
            auto tree_it = trees_.find(method);
            if (tree_it == trees_.end())
            {
                return std::nullopt;
            }

            return tree_it->second.search(path);
        }

        /**
         * @brief 检查是否空
         */
        [[nodiscard]] auto empty() const noexcept -> bool
        {
            return trees_.empty();
        }

    private:
        /**
         * @brief 添加路由
         */
        void add_route(http::verb method, std::string_view pattern, handler h)
        {
            auto& tree = trees_[method];
            tree.insert(pattern, std::move(h));
        }

        std::unordered_map<http::verb, radix_tree> trees_;
    };
}