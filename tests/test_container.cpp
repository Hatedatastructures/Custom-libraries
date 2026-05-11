// SPDX-License-Identifier: MIT
#define BOOST_TEST_MODULE ContainerTests
#include <boost/test/included/unit_test.hpp>
#include "wan/container/container.hpp"

BOOST_AUTO_TEST_SUITE(simulate_vector_tests)

BOOST_AUTO_TEST_CASE(push_back_and_size) {
    standard_con::vector<int> vec;
    vec.push_back(1);
    vec.push_back(2);
    BOOST_CHECK_EQUAL(vec.size(), 2);
}

BOOST_AUTO_TEST_SUITE_END()
