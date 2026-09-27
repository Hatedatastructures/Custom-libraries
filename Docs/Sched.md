# Sched 调度与线程池模块

[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](../LICENSE)

## 概览

当前调度模块位于 `Wan/Sched/Pool.hpp`，是对 `boost::asio::thread_pool` 的轻量封装。旧版动态调度器、任务排名队列和工作线程层已经不属于当前源码树，本文只记录现有实现。

公开类型为 `Wan::Sched::Pool`，主要接口如下：

| 接口 | 作用 |
|---|---|
| `Pool(std::size_t Threads = 0)` | 创建线程池；传入 `0` 时使用硬件并发数 |
| `Executor()` | 获取 `boost::asio::any_io_executor` |
| `Post(F&&)` | 向线程池提交可调用任务 |
| `Stop()` | 请求尽快停止底层工作线程；尚未执行的任务可能不会运行 |
| `Join()` | 等待线程池中的任务完成 |
| `NativePool()` | 获取底层 `boost::asio::thread_pool` |

## 使用示例

```cpp
#include "Wan/Sched/Pool.hpp"

#include <atomic>

int main()
{
    Wan::Sched::Pool Pool{4};
    std::atomic_int completed = 0;

    Pool.Post([&completed] { ++completed; });
    Pool.Join();

    return completed == 1 ? 0 : 1;
}
```

## 生命周期

`Pool` 不可复制。`Post()` 直接把任务交给 Asio，没有独立的 stopped 状态或拒绝提交检查；调用 `Stop()` 后继续提交的任务不保证会执行。等待正常排空时调用 `Join()`；请求尽快停止时调用 `Stop()`，它可能使尚未执行的任务不再运行。需要运行 Asio 协程时，可通过 `Executor()` 获取执行器并调用 `boost::asio::co_spawn`。

## 依赖与测试

实现依赖 Boost.Asio，构建配置会为公共目标提供 Boost 头文件路径和 C++23 编译特性。对应测试位于 `Tests/TestSched.cpp`，通过以下命令运行：

```bash
ctest --test-dir build --output-on-failure -R TestSched
```
