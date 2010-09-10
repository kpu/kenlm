#include "util/key_value_packing.hh"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/scoped_array.hpp>
#define BOOST_TEST_MODULE KeyValueStoreTest
#include <boost/test/unit_test.hpp>

#include <limits>
#include <stdlib.h>

namespace util {
namespace {

BOOST_AUTO_TEST_CASE(basic_in_out) {
  typedef ByteAlignedPacking<uint64_t, unsigned char> Packing;
  void *backing = malloc(Packing::kBytes * 2);
  Packing::MutableIterator i(Packing::FromVoid(backing));
  i->SetKey(10);
  BOOST_CHECK_EQUAL(10, i->GetKey());
  i->SetValue(3);
  BOOST_CHECK_EQUAL(3, i->GetValue());
  ++i;
  i->SetKey(5);
  BOOST_CHECK_EQUAL(5, i->GetKey());
  i->SetValue(42);
  BOOST_CHECK_EQUAL(42, i->GetValue());

  Packing::ConstIterator c(i);
  BOOST_CHECK_EQUAL(5, c->GetKey());
  --c;
  BOOST_CHECK_EQUAL(10, c->GetKey());
  BOOST_CHECK_EQUAL(42, i->GetValue());

  BOOST_CHECK_EQUAL(5, i->GetKey());
  free(backing);
}

BOOST_AUTO_TEST_CASE(simple_sort) {
  typedef ByteAlignedPacking<uint64_t, unsigned char> Packing;
  char foo[Packing::kBytes * 4];
  Packing::MutableIterator begin(Packing::FromVoid(foo));
  Packing::MutableIterator i = begin;
  i->SetKey(0); ++i;
  i->SetKey(2); ++i;
  i->SetKey(3); ++i;
  i->SetKey(1); ++i;
  std::sort(begin, i);
  BOOST_CHECK_EQUAL(0, begin[0].GetKey());
  BOOST_CHECK_EQUAL(1, begin[1].GetKey());
  BOOST_CHECK_EQUAL(2, begin[2].GetKey());
  BOOST_CHECK_EQUAL(3, begin[3].GetKey());
}

BOOST_AUTO_TEST_CASE(big_sort) {
  typedef ByteAlignedPacking<uint64_t, unsigned char> Packing;
  boost::scoped_array<char> memory(new char[Packing::kBytes * 1000]);
  Packing::MutableIterator begin(Packing::FromVoid(memory.get()));

  boost::mt19937 rng;
  boost::uniform_int<uint64_t> range(0, std::numeric_limits<uint64_t>::max());
  boost::variate_generator<boost::mt19937&, boost::uniform_int<uint64_t> > gen(rng, range);

  for (size_t i = 0; i < 1000; ++i) {
    (begin + i)->SetKey(gen());
  }
  std::sort(begin, begin + 1000);
  for (size_t i = 0; i < 999; ++i) {
    BOOST_CHECK(begin[i] < begin[i+1]);
  }
}

} // namespace
} // namespace util
