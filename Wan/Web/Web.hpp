/**
 * @file Web.hpp
 * @brief Web 模块入口
 * @details 聚合 Web 框架所有公共头文件。
 * @author Hatedatastructures
 * @date 2026-05-25
 */
#pragma once

// 核心
#include "Wan/Web/Core/Context.hpp"
#include "Wan/Web/Core/Executor.hpp"
#include "Wan/Web/Core/Handler.hpp"
#include "Wan/Web/Core/Metrics.hpp"

// 应用生命周期
#include "Wan/Web/Application/Application.hpp"

// 路由
#include "Wan/Web/Router/Router.hpp"

// 中间件
#include "Wan/Web/Middleware/Middleware.hpp"
#include "Wan/Web/Middleware/Builtins.hpp"

// 服务器
#include "Wan/Web/Server/Server.hpp"
#include "Wan/Web/Server/TlsServer.hpp"
#include "Wan/Web/Server/Static.hpp"

// 客户端
#include "Wan/Web/Client/Client.hpp"

// Session
#include "Wan/Web/Session/Session.hpp"
