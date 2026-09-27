// SPDX-License-Identifier: MIT
#define BOOST_TEST_MODULE NetworkTests
#include <boost/test/included/unit_test.hpp>
#include "Wan/Network/Agreement/Protocol.hpp"

BOOST_AUTO_TEST_SUITE(ProtocolTests)

BOOST_AUTO_TEST_CASE(DefaultConstruction) {
    Protocol::RequestHeader Req;
    BOOST_CHECK(true);  // 基本构造测试
}

BOOST_AUTO_TEST_SUITE_END()
