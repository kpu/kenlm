#include "util/sized_iterator.hh"

#define BOOST_TEST_MODULE SizedIteratorTest
#include <boost/test/unit_test.hpp>

namespace util { namespace {

BOOST_AUTO_TEST_CASE(swap_works) {
  char str[2] = { 0, 1 };
  SizedProxy first(str, 1), second(str + 1, 1);
  swap(first, second);
  BOOST_CHECK_EQUAL(1, str[0]);
  BOOST_CHECK_EQUAL(0, str[1]);
}

}} // namespace anonymous util
