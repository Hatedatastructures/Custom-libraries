// SPDX-License-Identifier: MIT
#define BOOST_TEST_MODULE SchedTests
#include <boost/test/included/unit_test.hpp>
#include "wan/sched/pool.hpp"

BOOST_AUTO_TEST_SUITE(thread_pool_tests)

BOOST_AUTO_TEST_CASE(submit_and_wait) {
    pool_config config;
    config.min_threads_ = 2;
    config.max_threads_ = 2;
    config.core_threads_ = 2;
    config.initial_threads_ = 2;
    wan::pool::thread_pool pool(config);
    std::atomic<int> counter{0};
    pool.submit([&counter]() { counter.fetch_add(1); });
    pool.submit([&counter]() { counter.fetch_add(1); });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    BOOST_CHECK_EQUAL(counter.load(), 2);
}

BOOST_AUTO_TEST_SUITE_END()
