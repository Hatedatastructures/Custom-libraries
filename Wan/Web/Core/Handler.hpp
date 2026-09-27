/**
 * @file Handler.hpp
 * @brief Web 请求处理器类型。
 */
#pragma once

#include "Wan/Web/Core/Context.hpp"

#include <boost/asio.hpp>

#include <functional>

namespace Wan::Web
{
    namespace Net = boost::asio;

    /** @brief 接收请求上下文并以 Asio 协程响应的处理器类型。 */
    using Handler = std::function<Net::awaitable<void>(Context&)>;
}
