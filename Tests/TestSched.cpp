/**
 * @file TestSched.cpp
 * @brief 线程池测试
 * @details 测试 Wan::Sched::Pool 的构造、任务提交、协程支持和并发执行。
 * @author Hatedatastructures
 * @date 2026-05-25
 */

#include <boost/asio.hpp>

#include <atomic>
#include <cstdio>
#include <mutex>
#include <set>
#include <thread>

#include "../Wan/Sched/Pool.hpp"

namespace Net = boost::asio;

static auto TestConstructAndJoin() -> bool
{
    Wan::Sched::Pool P{2};
    P.Join();
    std::puts("PASS: construct_and_join");
    return true;
}

static auto TestPostTask() -> bool
{
    Wan::Sched::Pool P{2};
    std::atomic<int> Counter{0};

    P.Post([&]() { Counter.fetch_add(1); });
    P.Post([&]() { Counter.fetch_add(1); });
    P.Post([&]() { Counter.fetch_add(1); });

    P.Join();

    if (Counter.load() != 3)
    {
        std::printf("FAIL: post_task - expected 3, got %d\n",
            Counter.load());
        return false;
    }
    std::puts("PASS: post_task");
    return true;
}

static auto TestConcurrentThreads() -> bool
{
    Wan::Sched::Pool P{4};
    std::mutex Mtx;
    std::set<std::thread::id> Ids;
    std::atomic<int> Done{0};

    for (int I = 0; I < 8; ++I)
    {
        P.Post([&]() {
            std::lock_guard<std::mutex> Lock{Mtx};
            Ids.insert(std::this_thread::get_id());
            Done.fetch_add(1);
        });
    }

    P.Join();

    if (Done.load() != 8)
    {
        std::printf("FAIL: concurrent_threads - done=%d expected 8\n",
            Done.load());
        return false;
    }

    // 应该有多个不同线程参与执行
    if (Ids.size() < 2)
    {
        std::printf("FAIL: concurrent_threads - only %zu threads used\n",
            Ids.size());
        return false;
    }

    std::printf("PASS: concurrent_threads (%zu threads used)\n", Ids.size());
    return true;
}

static auto TestCoroutineSupport() -> bool
{
    Wan::Sched::Pool P{2};
    std::atomic<bool> CoroDone{false};

    Net::co_spawn(P.Executor(),
        [&CoroDone]() -> Net::awaitable<void>
        {
            auto Ex = co_await Net::this_coro::executor;
            Net::steady_timer T(Ex, std::chrono::milliseconds(10));
            co_await T.async_wait(Net::use_awaitable);
            CoroDone.store(true);
        },
        Net::detached);

    P.Join();

    if (!CoroDone.load())
    {
        std::puts("FAIL: coroutine_support");
        return false;
    }
    std::puts("PASS: coroutine_support");
    return true;
}

static auto TestStop() -> bool
{
    Wan::Sched::Pool P{2};
    P.Stop();
    P.Join();
    std::puts("PASS: stop");
    return true;
}

auto main() -> int // NOLINT(bugprone-exception-escape)
{
    int Failed = 0;

    if (!TestConstructAndJoin()) ++Failed;
    if (!TestPostTask()) ++Failed;
    if (!TestConcurrentThreads()) ++Failed;
    if (!TestCoroutineSupport()) ++Failed;
    if (!TestStop()) ++Failed;

    std::printf("\n%d/%d tests passed\n", 5 - Failed, 5);
    return Failed;
}
