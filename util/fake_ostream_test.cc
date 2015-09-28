#define BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
#define BOOST_TEST_MODULE FakeOStreamTest

#include "util/fake_ostream.hh"
#include <boost/test/unit_test.hpp>
#include <boost/lexical_cast.hpp>

#include <cstddef>
#include <limits>

namespace util { namespace {

template <class T> void TestEqual(const T value) {
  std::string str;
  FakeSStream(str) << value;
  BOOST_CHECK_EQUAL(boost::lexical_cast<std::string>(value), str);
}

template <class T> void TestCorners() {
  TestEqual(std::numeric_limits<T>::max());
  TestEqual(std::numeric_limits<T>::min());
  TestEqual(static_cast<T>(0));
  TestEqual(static_cast<T>(-1));
  TestEqual(static_cast<T>(1));
}

BOOST_AUTO_TEST_CASE(Integer) {
  TestCorners<char>();
  TestCorners<signed char>();
  TestCorners<unsigned char>();

  TestCorners<short>();
  TestCorners<signed short>();
  TestCorners<unsigned short>();

  TestCorners<int>();
  TestCorners<unsigned int>();
  TestCorners<signed int>();

  TestCorners<long>();
  TestCorners<unsigned long>();
  TestCorners<signed long>();

  TestCorners<long long>();
  TestCorners<unsigned long long>();
  TestCorners<signed long long>();

  TestCorners<std::size_t>();
}

enum TinyEnum { EnumValue };

BOOST_AUTO_TEST_CASE(EnumCase) {
  TestEqual(EnumValue);
}

}} // namespaces
