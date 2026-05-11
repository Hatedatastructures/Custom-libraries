// SPDX-License-Identifier: MIT
#define BOOST_TEST_MODULE NetworkTests
#include <boost/test/included/unit_test.hpp>
#include "wan/network/agreement/protocol.hpp"

BOOST_AUTO_TEST_SUITE(protocol_tests)

BOOST_AUTO_TEST_CASE(default_construction) {
    protocol::request_header req;
    BOOST_CHECK(true);  // 基本构造测试
}

BOOST_AUTO_TEST_SUITE_END()
