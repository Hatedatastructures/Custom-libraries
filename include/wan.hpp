/**
 * @file Wan.hpp
 * @brief 组件库统一入口
 * @details wan 组件库，高性能 C++20 模块
 * @author Hatedatastructures
 * @date 2026-05-11
 */
#pragma once

// 日志模块 - 协程驱动
#include "../wan/chronicle/log.hpp"

// 容器模块 - STL 模拟实现
#include "../wan/container/container.hpp"

// 网络模块 - 协议与加密
#include "../wan/network/network.hpp"

// Web 模块 - 协程驱动 Web 框架
#include "../wan/web/web.hpp"

namespace wan
{
    /**
     * @brief wan 组件库
     * @details 高性能 C++20 模块库
     * @note 主要模块：
     *   - wan::chronicle - 协程日志
     *   - wan::container - STL 容器模拟
     *   - wan::network - 协议与加密
     *   - wan::web - 协程 Web 框架
     */
}