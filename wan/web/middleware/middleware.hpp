/**
 * @file middleware.hpp
 * @brief 中间件基类
 * @details 定义中间件类型和接口。
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

#include <boost/asio.hpp>
#include <functional>

#include <wan/web/core/context.hpp>

namespace wan::web
{
    namespace net = boost::asio;

    /**
     * @brief 中间件类型
     * @details 处理请求并调用下一个处理器。
     */
    using middleware = std::function<net::awaitable<void>(context&, std::function<net::awaitable<void>()>)>;
}