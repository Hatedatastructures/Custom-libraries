/**
 * @file Context.hpp
 * @brief HTTP 请求上下文
 * @details 封装 HTTP 请求和响应，提供路由参数、查询参数、Cookie 和响应构建。
 *          SetCookie 使用 insert() 而非 set() 以支持多 Cookie。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

#include <boost/beast.hpp>

#include <cctype>
#include <chrono>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>

namespace Wan::Web
{
    namespace Beast = boost::beast;
    namespace Http = Beast::http;

    /**
     * @struct CookieOptions
     * @brief Cookie 设置选项
     */
    struct CookieOptions
    {
        /** @brief 最大存活时长；零值时不写入 Max-Age。 */
        std::chrono::seconds MaxAge{0};
        /** @brief Cookie 路径；为空时不写入 Path。 */
        std::string Path = "/";
        /** @brief Cookie 域；为空时不写入 Domain。 */
        std::string Domain;
        /** @brief 是否添加 Secure 属性。 */
        bool Secure = false;
        /** @brief 是否添加 HttpOnly 属性。 */
        bool HttpOnly = false;
        /** @brief SameSite 属性值；为空时不写入该属性。 */
        std::string SameSite;
    };

    /**
     * @class Context
     * @brief 请求上下文
     * @details 封装 HTTP 请求和响应，每连接每请求创建。
     */
    class Context
    {
    public:
        /** @brief Beast 字符串正文类型。 */
        using StringBody = Http::string_body;
        /** @brief 本上下文使用的 HTTP 请求类型。 */
        using RequestType = Http::request<StringBody>;
        /** @brief 本上下文使用的 HTTP 响应类型。 */
        using ResponseType = Http::response<StringBody>;

        /**
         * @brief 从 HTTP 请求创建上下文。
         * @param Req 要接管的 HTTP 请求。
         */
        explicit Context(RequestType&& Req)
            : MemberRequest(std::move(Req))
            , MemberResponse(Http::status::ok, MemberRequest.version())
        {
            MemberResponse.set(Http::field::server, "wan/web");
            MemberResponse.set(Http::field::content_type, "text/plain");
            ParseQuery();
        }

        // ── 请求访问 ──

        /**
         * @brief 获取 HTTP 方法。
         * @return Beast HTTP 方法枚举。
         */
        [[nodiscard]] auto Method() const noexcept -> Http::verb
        {
            return MemberRequest.method();
        }

        /**
         * @brief 获取 HTTP 方法文本视图。
         * @return 指向请求内部数据的非拥有视图。
         */
        [[nodiscard]] auto MethodString() const -> std::string_view
        {
            return MemberRequest.method_string();
        }

        /**
         * @brief 获取原始请求目标视图，包含可能存在的查询串。
         * @return 指向请求内部数据的非拥有视图。
         */
        [[nodiscard]] auto Target() const noexcept -> std::string_view
        {
            return MemberRequest.target();
        }

        /**
         * @brief 获取请求正文视图。
         * @return 指向请求内部数据的非拥有视图。
         */
        [[nodiscard]] auto Body() const -> std::string_view
        {
            return MemberRequest.body();
        }

        /**
         * @brief 查找请求头字段。
         * @param Name 字段名。
         * @return 字段值视图；字段不存在时为空视图。视图由此上下文持有。
         */
        [[nodiscard]] auto Header(std::string_view Name) const noexcept
            -> std::string_view
        {
            auto It = MemberRequest.find(Name);
            if (It != MemberRequest.end())
            {
                return It->value();
            }
            return {};
        }

        /**
         * @brief 检查请求是否启用 Keep-Alive。
         * @return 启用时为 `true`。
         */
        [[nodiscard]] auto KeepAlive() const noexcept -> bool
        {
            return MemberRequest.keep_alive();
        }

        /**
         * @brief 获取 HTTP 版本号。
         * @return 请求使用的版本号。
         */
        [[nodiscard]] auto Version() const noexcept -> unsigned int
        {
            return MemberRequest.version();
        }

        // ── 路由参数 ──

        /**
         * @brief 设置路由参数（由路由器调用）
         * @param Params 参数名和值的映射。
         */
        void SetParams(std::map<std::string, std::string> Params)
        {
            MemberParams = std::move(Params);
        }

        /**
         * @brief 获取路由参数。
         * @param Name 参数名。
         * @return 参数值；参数不存在时返回空字符串。
         */
        [[nodiscard]] auto Param(const std::string& Name) const
            -> std::string
        {
            auto It = MemberParams.find(Name);
            if (It != MemberParams.end())
            {
                return It->second;
            }
            return {};
        }

        /**
         * @brief 检查路由参数是否存在。
         * @param Name 参数名。
         * @return 参数存在时为 `true`。
         */
        [[nodiscard]] auto HasParam(const std::string& Name) const -> bool
        {
            return MemberParams.find(Name) != MemberParams.end();
        }

        // ── 查询参数 ──

        /**
         * @brief 获取查询参数。
         * @param Name 参数名。
         * @return 参数值；参数不存在时返回空字符串。
         */
        [[nodiscard]] auto Query(std::string_view Name) const
            -> std::string
        {
            auto It = MemberQueryParams.find(std::string(Name));
            if (It != MemberQueryParams.end())
            {
                return It->second;
            }
            return {};
        }

        /**
         * @brief 检查查询参数是否存在。
         * @param Name 参数名。
         * @return 参数存在时为 `true`。
         */
        [[nodiscard]] auto HasQuery(std::string_view Name) const -> bool
        {
            return MemberQueryParams.find(std::string(Name)) != MemberQueryParams.end();
        }

        // ── Cookie ──

        /**
         * @brief 获取请求 Cookie。
         * @param Name Cookie 名称。
         * @return Cookie 值视图；Cookie 不存在时为空视图。视图由此上下文持有。
         */
        [[nodiscard]] auto Cookie(std::string_view Name) -> std::string_view
        {
            ParseCookies();
            auto It = MemberCookies.find(std::string(Name));
            if (It != MemberCookies.end())
            {
                return It->second;
            }
            return {};
        }

        /**
         * @brief 检查请求 Cookie 是否存在。
         * @param Name Cookie 名称。
         * @return Cookie 存在时为 `true`。
         */
        [[nodiscard]] auto HasCookie(std::string_view Name) -> bool
        {
            ParseCookies();
            return MemberCookies.find(std::string(Name)) != MemberCookies.end();
        }

        /**
         * @brief 添加响应 Cookie。
         * @param Name Cookie 名称。
         * @param Value Cookie 值。
         * @param Opts Cookie 属性。
         * @throws std::invalid_argument 任一字段包含不允许的分隔符或控制字符时抛出。
         * @note 使用独立的 `Set-Cookie` 字段，以支持多个 Cookie。
         */
        void SetCookie(std::string_view Name, std::string_view Value,
            const CookieOptions& Opts = {})
        {
            auto HasForbidden = [](std::string_view Text,
                std::string_view Forbidden) {
                return Text.find_first_of(Forbidden)
                    != std::string_view::npos;
            };
            auto HasControl = [](std::string_view Text) {
                for (const auto Character : Text)
                {
                    const auto Value = static_cast<unsigned char>(Character);
                    if (Value < 0x20 || Value == 0x7f)
                    {
                        return true;
                    }
                }
                return false;
            };
            if (Name.empty() || HasForbidden(Name, "=;,\r\n")
                || HasForbidden(Value, ";\r\n")
                || HasForbidden(Opts.Path, ";\r\n")
                || HasForbidden(Opts.Domain, ";,\r\n")
                || HasForbidden(Opts.SameSite, ";,\r\n")
                || HasControl(Name) || HasControl(Value)
                || HasControl(Opts.Path) || HasControl(Opts.Domain)
                || HasControl(Opts.SameSite))
            {
                throw std::invalid_argument("invalid cookie component");
            }

            std::string CookieStr =
                std::string(Name) + "=" + std::string(Value);

            if (Opts.MaxAge.count() != 0)
            {
                CookieStr +=
                    "; Max-Age=" + std::to_string(Opts.MaxAge.count());
            }
            if (!Opts.Path.empty())
            {
                CookieStr += "; Path=" + Opts.Path;
            }
            if (!Opts.Domain.empty())
            {
                CookieStr += "; Domain=" + Opts.Domain;
            }
            if (Opts.Secure)
            {
                CookieStr += "; Secure";
            }
            if (Opts.HttpOnly)
            {
                CookieStr += "; HttpOnly";
            }
            if (!Opts.SameSite.empty())
            {
                CookieStr += "; SameSite=" + Opts.SameSite;
            }

            MemberResponse.insert(Http::field::set_cookie, CookieStr);
        }

        // ── 响应构建 ──

        /**
         * @brief 设置 JSON 响应正文。
         * @param BodyStr JSON 文本。
         */
        void Json(std::string_view BodyStr)
        {
            MemberResponse.set(Http::field::content_type, "application/json");
            MemberResponse.body() = std::string(BodyStr);
            MemberResponse.prepare_payload();
        }

        /**
         * @brief 设置带状态码的 JSON 响应。
         * @param Status HTTP 状态码。
         * @param BodyStr JSON 文本。
         */
        void Json(Http::status Status, std::string_view BodyStr)
        {
            MemberResponse.result(Status);
            Json(BodyStr);
        }

        /**
         * @brief 设置纯文本响应正文。
         * @param BodyStr 文本正文。
         */
        void Text(std::string_view BodyStr)
        {
            MemberResponse.set(Http::field::content_type, "text/plain");
            MemberResponse.body() = std::string(BodyStr);
            MemberResponse.prepare_payload();
        }

        /**
         * @brief 设置带状态码的纯文本响应。
         * @param Status HTTP 状态码。
         * @param BodyStr 文本正文。
         */
        void Text(Http::status Status, std::string_view BodyStr)
        {
            MemberResponse.result(Status);
            Text(BodyStr);
        }

        /**
         * @brief 设置 HTML 响应正文。
         * @param BodyStr HTML 文本。
         */
        void Html(std::string_view BodyStr)
        {
            MemberResponse.set(Http::field::content_type, "text/html");
            MemberResponse.body() = std::string(BodyStr);
            MemberResponse.prepare_payload();
        }

        /**
         * @brief 设置响应状态码。
         * @param StatusValue HTTP 状态码。
         */
        void Status(Http::status StatusValue)
        {
            MemberResponse.result(StatusValue);
        }

        /**
         * @brief 设置响应头字段。
         * @param Name 字段名。
         * @param Value 字段值。
         */
        void SetHeader(std::string_view Name, std::string_view Value)
        {
            MemberResponse.set(Name, Value);
        }

        // ── 原始访问 ──

        /**
         * @brief 获取底层 HTTP 请求。
         * @return 底层请求的可写引用。
         */
        [[nodiscard]] auto Request() noexcept -> RequestType&
        {
            return MemberRequest;
        }

        /**
         * @brief 获取底层 HTTP 响应。
         * @return 底层响应的可写引用。
         */
        [[nodiscard]] auto Response() noexcept -> ResponseType&
        {
            return MemberResponse;
        }

    private:
        /**
         * @brief 解码一个查询串组件。
         * @param Value 原始查询串组件。
         * @return 将 `+` 转为空格并解码有效 `%HH` 转义后的字符串；无效转义原样保留。
         */
        [[nodiscard]] static auto DecodeQueryComponent(
            std::string_view Value) -> std::string
        {
            auto HexValue = [](char C) -> int
            {
                if (C >= '0' && C <= '9') return C - '0';
                if (C >= 'a' && C <= 'f') return C - 'a' + 10;
                if (C >= 'A' && C <= 'F') return C - 'A' + 10;
                return -1;
            };

            std::string Decoded;
            Decoded.reserve(Value.size());
            for (std::size_t I = 0; I < Value.size(); ++I)
            {
                if (Value[I] == '+')
                {
                    Decoded.push_back(' ');
                }
                else if (Value[I] == '%' && I + 2 < Value.size())
                {
                    const auto High = HexValue(Value[I + 1]);
                    const auto Low = HexValue(Value[I + 2]);
                    if (High >= 0 && Low >= 0)
                    {
                        Decoded.push_back(static_cast<char>(
                            (High << 4) | Low));
                        I += 2;
                    }
                    else
                    {
                        Decoded.push_back(Value[I]);
                    }
                }
                else
                {
                    Decoded.push_back(Value[I]);
                }
            }
            return Decoded;
        }

        /** @brief 从请求目标解析查询参数并写入 MemberQueryParams。 */
        void ParseQuery()
        {
            auto Target = MemberRequest.target();
            auto QueryPos = Target.find('?');
            if (QueryPos == std::string_view::npos)
            {
                return;
            }

            auto QueryStr = Target.substr(QueryPos + 1);
            while (!QueryStr.empty())
            {
                auto EqualsPos = QueryStr.find('=');
                auto AmpersandPos = QueryStr.find('&');

                std::string Key;
                std::string Value;

                if (EqualsPos != std::string_view::npos &&
                    (AmpersandPos == std::string_view::npos
                        || EqualsPos < AmpersandPos))
                {
                    Key = DecodeQueryComponent(
                        QueryStr.substr(0, EqualsPos));
                    auto ValueEnd = (AmpersandPos != std::string_view::npos)
                        ? AmpersandPos : QueryStr.size();
                    Value = DecodeQueryComponent(QueryStr.substr(
                        EqualsPos + 1, ValueEnd - EqualsPos - 1));
                }
                else
                {
                    auto SegmentEnd = (AmpersandPos != std::string_view::npos)
                        ? AmpersandPos : QueryStr.size();
                    Key = DecodeQueryComponent(
                        QueryStr.substr(0, SegmentEnd));
                }

                MemberQueryParams[std::move(Key)] = std::move(Value);

                if (AmpersandPos != std::string_view::npos)
                {
                    QueryStr = QueryStr.substr(AmpersandPos + 1);
                }
                else
                {
                    break;
                }
            }
        }

        /** @brief 首次访问时解析 Cookie 头并缓存结果。 */
        void ParseCookies()
        {
            if (MemberCookiesParsed)
            {
                return;
            }
            MemberCookiesParsed = true;

            auto CookieHeader = MemberRequest.find(Http::field::cookie);
            if (CookieHeader == MemberRequest.end())
            {
                return;
            }

            auto CookieString = CookieHeader->value();
            while (!CookieString.empty())
            {
                auto EqualsPos = CookieString.find('=');
                if (EqualsPos == std::string_view::npos)
                {
                    break;
                }
                auto SemicolonPos = CookieString.find(';');
                auto Name = std::string(
                    CookieString.substr(0, EqualsPos));
                auto ValueEnd = (SemicolonPos != std::string_view::npos)
                    ? SemicolonPos : CookieString.size();
                auto Value = std::string(CookieString.substr(
                    EqualsPos + 1, ValueEnd - EqualsPos - 1));

                MemberCookies[std::move(Name)] = std::move(Value);

                if (SemicolonPos != std::string_view::npos)
                {
                    CookieString = CookieString.substr(SemicolonPos + 1);
                    while (!CookieString.empty() && CookieString.front() == ' ')
                    {
                        CookieString = CookieString.substr(1);
                    }
                }
                else
                {
                    break;
                }
            }
        }

        /** @brief 此上下文接管的 HTTP 请求。 */
        RequestType MemberRequest;
        /** @brief 正在构造的 HTTP 响应。 */
        ResponseType MemberResponse;
        /** @brief 路由器写入的路径参数。 */
        std::map<std::string, std::string> MemberParams;
        /** @brief 从请求目标解析出的查询参数。 */
        std::unordered_map<std::string, std::string> MemberQueryParams;
        /** @brief 从 Cookie 头解析出的名称和值。 */
        std::unordered_map<std::string, std::string> MemberCookies;
        /** @brief 标记 Cookie 头是否已经解析。 */
        bool MemberCookiesParsed = false;
    };
}
