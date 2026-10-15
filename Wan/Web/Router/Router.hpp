/**
 * @file Router.hpp
 * @brief 基于基数树的路由器。
 * @details 按 HTTP 方法分别匹配路由，支持字符串、整数和剩余路径参数。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <cctype>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <unordered_map>

#include "Wan/Web/Core/Context.hpp"
#include "Wan/Web/Core/Handler.hpp"

namespace Wan::Web
{
    namespace Net = boost::asio;
    namespace Http = boost::beast::http;

    /** @brief 路由匹配结果及捕获的路径参数。 */
    struct RouteMatch
    {
        /** @brief 匹配到的请求处理器。 */
        Wan::Web::Handler Handler;
        /** @brief 参数名到捕获值的映射。 */
        std::map<std::string, std::string> Params;
    };

    /** @brief 路由参数的匹配方式。 */
    enum class ParamType : std::uint8_t
    {
        None, ///< 普通路径节点，不捕获参数。
        IntParam, ///< 匹配不含斜杠的整数参数。
        StringParam, ///< 匹配不含斜杠的字符串参数。
        PathParam ///< 匹配路径剩余部分，可包含斜杠。
    };

    /** @brief 基数树路由节点。 */
    class RadixNode : public std::enable_shared_from_this<RadixNode>
    {
    public:
        /** @brief 节点对应的静态路径片段。 */
        std::string Prefix;
        /** @brief 子节点列表。 */
        std::vector<std::shared_ptr<RadixNode>> Children;
        /** @brief 此节点关联的请求处理器。 */
        Wan::Web::Handler Handler;
        /** @brief 此节点的参数匹配类型。 */
        ParamType ParameterType = ParamType::None;
        /** @brief 参数节点使用的参数名。 */
        std::string ParamName;
        /** @brief 是否为通配路径参数。 */
        bool IsWildcard = false;

        /**
         * @brief 检查节点是否含有处理器。
         * @return 含有处理器时为 `true`。
         */
        [[nodiscard]] auto IsLeaf() const noexcept -> bool
        {
            return static_cast<bool>(Handler);
        }

        /**
         * @brief 检查节点是否表示路径参数。
         * @return 参数类型不是 `None` 时为 `true`。
         */
        [[nodiscard]] auto IsParam() const noexcept -> bool
        {
            return ParameterType != ParamType::None;
        }
    };

    /** @brief 存储并匹配一组路由路径的基数树。 */
    class RadixTree
    {
    public:
        /** @brief 创建空路由树。 */
        RadixTree()
            : MemberRoot(std::make_shared<RadixNode>())
        {
        }

        /**
         * @brief 注册路径模式及其处理器。
         * @param Path 以 `/` 开头的路径模式；支持 `<name>`、`<int:name>`、
         *        `<string:name>` 和末尾的 `<path:name>`。
         * @param H 请求处理器。
         * @throws std::invalid_argument 路径格式无效或处理器为空时抛出。
         */
        void Insert(std::string_view Path, Handler H)
        {
            if (Path.empty() || Path.front() != '/')
            {
                throw std::invalid_argument(
                    "route pattern must be an absolute path");
            }
            if (!H)
            {
                throw std::invalid_argument("route handler must not be empty");
            }

            // Validate the complete pattern before touching the radix tree.
            // This keeps malformed registrations from leaving partial nodes.
            std::unordered_set<std::string> ParameterNames;
            for (std::size_t Position = 0; Position < Path.size();)
            {
                if (Path[Position] != '<')
                {
                    if (Path[Position] == '>')
                    {
                        throw std::invalid_argument(
                            "unmatched route parameter terminator");
                    }
                    ++Position;
                    continue;
                }

                const auto End = Path.find('>', Position);
                if (End == std::string_view::npos)
                {
                    throw std::invalid_argument(
                        "unterminated route parameter");
                }
                const auto ParamSpec = Path.substr(
                    Position + 1, End - Position - 1);
                const auto ColonPos = ParamSpec.find(':');
                const auto Type = ColonPos == std::string_view::npos
                    ? std::string_view{"string"}
                    : ParamSpec.substr(0, ColonPos);
                const auto Name = ColonPos == std::string_view::npos
                    ? ParamSpec
                    : ParamSpec.substr(ColonPos + 1);
                if ((Type != "int" && Type != "string" && Type != "path")
                    || Name.empty())
                {
                    throw std::invalid_argument(
                        "invalid route parameter specification");
                }
                if (Type == "path" && End + 1 < Path.size())
                {
                    throw std::invalid_argument(
                        "path route parameter must be terminal");
                }
                for (const auto Character : Name)
                {
                    if (!std::isalnum(static_cast<unsigned char>(Character))
                        && Character != '_')
                    {
                        throw std::invalid_argument(
                            "invalid route parameter name");
                    }
                }
                if (!ParameterNames.emplace(Name).second)
                {
                    throw std::invalid_argument(
                        "duplicate route parameter name");
                }
                Position = End + 1;
            }

            auto Node = MemberRoot;
            std::size_t I = 0;

            while (I < Path.size())
            {
                if (Path[I] == '<')
                {
                    auto End = Path.find('>', I);
                    if (End == std::string_view::npos)
                    {
                        throw std::invalid_argument(
                            "unterminated route parameter");
                    }

                    auto ParamSpec = Path.substr(I + 1, End - I - 1);
                    auto ColonPos = ParamSpec.find(':');
                    std::string TypeStr;
                    std::string Name;

                    if (ColonPos != std::string_view::npos)
                    {
                        TypeStr = std::string(
                            ParamSpec.substr(0, ColonPos));
                        Name = std::string(
                            ParamSpec.substr(ColonPos + 1));
                    }
                    else
                    {
                        TypeStr = "string";
                        Name = std::string(ParamSpec);
                    }

                    if (Name.empty())
                    {
                        throw std::invalid_argument(
                            "route parameter name must not be empty");
                    }

                    if (TypeStr != "int" && TypeStr != "string"
                        && TypeStr != "path")
                    {
                        throw std::invalid_argument(
                            "invalid route parameter type");
                    }

                    auto ParamNode =
                        std::make_shared<RadixNode>();
                    ParamNode->ParamName = Name;

                    if (TypeStr == "int")
                    {
                        ParamNode->ParameterType =
                            ParamType::IntParam;
                    }
                    else if (TypeStr == "path")
                    {
                        ParamNode->ParameterType =
                            ParamType::PathParam;
                        ParamNode->IsWildcard = true;
                    }
                    else
                    {
                        ParamNode->ParameterType =
                            ParamType::StringParam;
                    }

                    bool Found = false;
                    for (auto& Child : Node->Children)
                    {
                        if (Child->IsParam()
                            && Child->ParamName == Name
                            && Child->ParameterType
                                != ParamNode->ParameterType)
                        {
                            throw std::invalid_argument(
                                "conflicting parameter types for route shape");
                        }
                        if (Child->IsParam()
                            && Child->ParameterType == ParamNode->ParameterType)
                        {
                            if (Child->ParamName != Name)
                            {
                                throw std::invalid_argument(
                                    "conflicting parameter names for route shape");
                            }
                            Found = true;
                            Node = Child;
                            break;
                        }
                    }

                    if (!Found)
                    {
                        Node->Children.push_back(ParamNode);
                        Node = ParamNode;
                    }

                    I = End + 1;
                }
                else
                {
                    // 取当前位置到下一个 '<' 或路径末尾
                    auto NextParam = Path.find('<', I);
                    auto Segment =
                        (NextParam != std::string_view::npos)
                            ? Path.substr(I, NextParam - I)
                            : Path.substr(I);

                    std::shared_ptr<RadixNode> Matched;
                    std::size_t CommonLen = 0;

                    for (auto& Child : Node->Children)
                    {
                        if (Child->IsParam())
                        {
                            continue;
                        }
                        std::size_t C = 0;
                        while (C < Child->Prefix.size()
                               && C < Segment.size()
                               && Child->Prefix[C] == Segment[C])
                        {
                            ++C;
                        }
                        if (C > 0)
                        {
                            Matched = Child;
                            CommonLen = C;
                            break;
                        }
                    }

                    if (Matched)
                    {
                        if (CommonLen < Matched->Prefix.size())
                        {
                            auto Branch =
                                std::make_shared<RadixNode>();
                            Branch->Prefix =
                                Matched->Prefix.substr(CommonLen);
                            Branch->Children =
                                std::move(Matched->Children);
                            Branch->Handler =
                                std::move(Matched->Handler);

                            Matched->Prefix =
                                Matched->Prefix.substr(
                                    0, CommonLen);
                            Matched->Children = {Branch};
                            Matched->Handler = nullptr;
                        }
                        Node = Matched;
                        I += CommonLen;
                    }
                    else
                    {
                        auto NewNode =
                            std::make_shared<RadixNode>();
                        NewNode->Prefix = std::string(Segment);
                        Node->Children.push_back(NewNode);
                        Node = NewNode;
                        I += Segment.size();
                    }
                }
            }

            Node->Handler = std::move(H);
        }

        /**
         * @brief 在路由树中查找路径。
         * @param Path 不含查询串的请求路径。
         * @return 找到时返回处理器和捕获参数，否则返回空值。
         */
        [[nodiscard]] auto Search(std::string_view Path) const
            -> std::optional<RouteMatch>
        {
            std::map<std::string, std::string> Params;
            auto Result = SearchNode(MemberRoot, Path, 0, Params);

            if (Result)
            {
                RouteMatch Match;
                Match.Handler = Result->get()->Handler;
                Match.Params = std::move(Params);
                return Match;
            }

            return std::nullopt;
        }

    private:
        /**
         * @brief 从当前基数树节点继续匹配路径。
         * @param Node 当前待匹配的节点。
         * @param Path 完整请求路径。
         * @param Pos 当前路径偏移。
         * @param Params 接收匹配到的路径参数。
         * @return 匹配终点节点；未匹配时返回空值。
         */
        [[nodiscard]] auto SearchNode(
            const std::shared_ptr<RadixNode>& Node,
            std::string_view Path, std::size_t Pos,
            std::map<std::string, std::string>& Params) const
            -> std::optional<std::shared_ptr<RadixNode>>
        {
            // 匹配当前节点前缀
            if (!Node->Prefix.empty())
            {
                if (Pos + Node->Prefix.size() > Path.size())
                {
                    return std::nullopt;
                }
                if (Path.substr(Pos, Node->Prefix.size()) != Node->Prefix)
                {
                    return std::nullopt;
                }
                Pos += Node->Prefix.size();
            }

            // 路径末尾
            if (Pos >= Path.size())
            {
                if (Node->IsLeaf())
                {
                    return Node;
                }
                return std::nullopt;
            }

            // 优先匹配静态子节点
            for (auto& Child : Node->Children)
            {
                if (Child->IsParam())
                {
                    continue;
                }
                auto Result = SearchNode(Child, Path, Pos, Params);
                if (Result)
                {
                    return Result;
                }
            }

            // 尝试参数子节点
            for (auto& Child : Node->Children)
            {
                if (!Child->IsParam())
                {
                    continue;
                }

                std::string ParamValue;
                std::size_t NewPos = Pos;

                if (Child->ParameterType == ParamType::PathParam)
                {
                    ParamValue = std::string(Path.substr(Pos));
                    NewPos = Path.size();
                }
                else
                {
                    while (NewPos < Path.size() && Path[NewPos] != '/')
                    {
                        if (Child->ParameterType == ParamType::IntParam)
                        {
                            if (!std::isdigit(
                                    static_cast<unsigned char>(
                                        Path[NewPos])))
                            {
                                break;
                            }
                        }
                        ParamValue += Path[NewPos];
                        ++NewPos;
                    }

                    if (ParamValue.empty())
                    {
                        continue;
                    }
                }

                Params[Child->ParamName] = ParamValue;
                auto Result =
                    SearchNode(Child, Path, NewPos, Params);
                if (Result)
                {
                    return Result;
                }

                Params.erase(Child->ParamName);
            }

            return std::nullopt;
        }

        /** @brief 基数树根节点。 */
        std::shared_ptr<RadixNode> MemberRoot;
    };

    /** @brief 按 HTTP 方法管理并匹配路由。 */
    class Router
    {
    public:
        /** @brief 冻结路由表；此后注册路由会抛出异常。 */
        void Freeze() noexcept
        {
            MemberFrozen = true;
        }

        /**
         * @brief 注册 GET 路由。
         * @param Pattern 路径模式。
         * @param H 请求处理器。
         * @return 当前路由器，供链式调用。
         * @throws std::logic_error 路由器已冻结时抛出。
         * @throws std::invalid_argument 路径模式无效或处理器为空时抛出。
         */
        auto Get(std::string_view Pattern, Handler H) -> Router&
        {
            AddRoute(Http::verb::get, Pattern, std::move(H));
            return *this;
        }

        /**
         * @brief 注册 POST 路由。
         * @param Pattern 路径模式。
         * @param H 请求处理器。
         * @return 当前路由器，供链式调用。
         * @throws std::logic_error 路由器已冻结时抛出。
         * @throws std::invalid_argument 路径模式无效或处理器为空时抛出。
         */
        auto Post(std::string_view Pattern, Handler H) -> Router&
        {
            AddRoute(Http::verb::post, Pattern, std::move(H));
            return *this;
        }

        /**
         * @brief 注册 PUT 路由。
         * @param Pattern 路径模式。
         * @param H 请求处理器。
         * @return 当前路由器，供链式调用。
         * @throws std::logic_error 路由器已冻结时抛出。
         * @throws std::invalid_argument 路径模式无效或处理器为空时抛出。
         */
        auto Put(std::string_view Pattern, Handler H) -> Router&
        {
            AddRoute(Http::verb::put, Pattern, std::move(H));
            return *this;
        }

        /**
         * @brief 注册 DELETE 路由。
         * @param Pattern 路径模式。
         * @param H 请求处理器。
         * @return 当前路由器，供链式调用。
         * @throws std::logic_error 路由器已冻结时抛出。
         * @throws std::invalid_argument 路径模式无效或处理器为空时抛出。
         */
        auto Del(std::string_view Pattern, Handler H) -> Router&
        {
            AddRoute(Http::verb::delete_, Pattern, std::move(H));
            return *this;
        }

        /**
         * @brief 注册 PATCH 路由。
         * @param Pattern 路径模式。
         * @param H 请求处理器。
         * @return 当前路由器，供链式调用。
         * @throws std::logic_error 路由器已冻结时抛出。
         * @throws std::invalid_argument 路径模式无效或处理器为空时抛出。
         */
        auto Patch(std::string_view Pattern, Handler H) -> Router&
        {
            AddRoute(Http::verb::patch, Pattern, std::move(H));
            return *this;
        }

        /**
         * @brief 按 HTTP 方法和请求目标查找路由。
         * @param Method HTTP 方法。
         * @param Target 请求目标；查询串会在匹配前移除。
         * @return 匹配到的处理器和参数；未匹配时返回空值。
         */
        [[nodiscard]] auto Match(Http::verb Method,
            std::string_view Target) const
            -> std::optional<RouteMatch>
        {
            std::string Path(Target);
            auto QueryPos = Path.find('?');
            if (QueryPos != std::string::npos)
            {
                Path = Path.substr(0, QueryPos);
            }

            auto TreeIt = MemberTrees.find(Method);
            if (TreeIt == MemberTrees.end())
            {
                return std::nullopt;
            }

            return TreeIt->second.Search(Path);
        }

        /**
         * @brief 检查路由器是否没有任何路由。
         * @return 没有路由时为 `true`。
         */
        [[nodiscard]] auto Empty() const noexcept -> bool
        {
            return MemberTrees.empty();
        }

    private:
        /**
         * @brief 将处理器注册到指定 HTTP 方法和路径。
         * @param Method 路由匹配的 HTTP 方法。
         * @param Pattern 路由路径模式。
         * @param H 命中路由时调用的处理器。
         * @throws std::logic_error 路由器已冻结时抛出。
         */
        void AddRoute(Http::verb Method, std::string_view Pattern,
            Handler H)
        {
            if (MemberFrozen)
            {
                throw std::logic_error(
                    "routes cannot be registered after the router is frozen");
            }
            auto It = MemberTrees.find(Method);
            if (It == MemberTrees.end())
            {
                RadixTree Tree;
                Tree.Insert(Pattern, std::move(H));
                MemberTrees.emplace(Method, std::move(Tree));
                return;
            }
            It->second.Insert(Pattern, std::move(H));
        }

        /** @brief 按 HTTP 方法保存的路由基数树。 */
        std::unordered_map<Http::verb, RadixTree> MemberTrees;
        /** @brief 标记路由表是否已冻结。 */
        bool MemberFrozen = false;
    };
}
