#include "util/log_num.hh"

#define BOOST_TEST_MODULE LogNumTest
#include <boost/test/unit_test.hpp>

#include <math.h>

namespace {

#define CHECK_LOGNUM_EQ(constant, value) \
  BOOST_CHECK_MESSAGE( \
		fabs((constant) - (value).Linear()) <= 0.00001 * (constant), \
      		(constant) << " - " << (value).Linear() << " is " << ((constant) - (value).Linear()) << "." \
      		<< " Tolerance = " << (0.00001 * (constant)) << "\n" \
		<< " in " << __LINE__)
  
BOOST_AUTO_TEST_CASE(construction) {
  CHECK_LOGNUM_EQ(0.0, LogNum<double>());
  CHECK_LOGNUM_EQ(1.2e08, LogNum<double>(1.2e08));
  CHECK_LOGNUM_EQ(2.718281828, LogNum<long double>(AlreadyLogTag(), 1.0));
}

BOOST_AUTO_TEST_CASE(addition) {
  CHECK_LOGNUM_EQ(5.0+3.0, LogNum<float>(5.0).UnstableAdd(LogNum<float>(3.0)));
  CHECK_LOGNUM_EQ(5.0+3.0, LogNum<double>(5.0).UnstableAdd(LogNum<double>(3.0)));
  CHECK_LOGNUM_EQ(1.0e120, LogNum<double>(1.0e120).UnstableAdd(LogNum<double>()));
  CHECK_LOGNUM_EQ(1.0e121, LogNum<long double>().UnstableAdd(LogNum<long double>(1.0e121)));
  CHECK_LOGNUM_EQ(0.0, LogNum<float>(0.0).UnstableAdd(LogNum<float>(0.0)));
  CHECK_LOGNUM_EQ(1.0, LogNum<long double>(0.5).UnstableAdd(LogNum<long double>(0.5)));
}

BOOST_AUTO_TEST_CASE(multiplication) {
  CHECK_LOGNUM_EQ(5.0*3.0, LogNum<double>(5.0) * LogNum<double>(3.0));
}

BOOST_AUTO_TEST_CASE(division) {
  CHECK_LOGNUM_EQ(3.0 / 0.0001, LogNum<float>(3.0) / LogNum<float>(0.0001));
}

BOOST_AUTO_TEST_CASE(power) {
  CHECK_LOGNUM_EQ(pow(1.01, 5.2), pow(LogNum<double>(1.01), 5.2));
  CHECK_LOGNUM_EQ(pow(1.01, 5.2) * pow(3.14, 1.3) * pow(8.3, 7.0),
		   pow(LogDouble(1.01), 5.2) * pow(LogDouble(3.14), 1.3) * pow(LogDouble(8.3), 7.0));
}

} // namespace
