/* Program to verify compilation against the unit test framework */

#define BOOST_TEST_MODULE EmptyTest
#include <boost/test/unit_test.hpp>

namespace {
BOOST_AUTO_TEST_CASE(Empty) {}
} // namespace
