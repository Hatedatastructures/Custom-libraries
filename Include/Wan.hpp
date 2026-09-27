/**
 * @file Wan.hpp
 * @brief 组件库统一入口
 * @details Wan 组件库，高性能 C++23 模块
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

// 日志模块 - 协程驱动
#include "../Wan/Chronicle/Log.hpp"

// 容器模块 - STL 模拟实现
#include "../Wan/Container/Container.hpp"

// 网络模块 - 协议与加密
#include "../Wan/Network/Network.hpp"

// JSON 工具模块
#include "../Wan/Json/Json.hpp"

// 错误码模块
#include "../Wan/Fault/Fault.hpp"

// 传输层模块
#include "../Wan/Transport/Transport.hpp"

// 调度模块
#include "../Wan/Sched/Pool.hpp"

// Web 框架模块
#include "../Wan/Web/Web.hpp"

namespace Wan
{
    /**
     * @brief Wan 组件库
     * @details 高性能 C++23 模块库
     * @note 主要模块：
     *   - Wan::Chronicle - 协程日志
     *   - Wan::Container - STL 容器模拟
     *   - Wan::Network - 协议与加密
     *   - Wan::Fault - 错误码
     *   - Wan::Transport - 传输层抽象
     *   - Wan::Sched - 线程池调度
     *   - Wan::Web - 协程 Web 框架
     */
}
