#include "lm/interpolate/bounded_sequence_encoding.hh"

#include "util/scoped.hh"

#define BOOST_TEST_MODULE BoundedSequenceEncodingTest
#include <boost/test/unit_test.hpp>

namespace lm {
namespace interpolate {

void ExhaustiveTest(unsigned char *bound_begin, unsigned char *bound_end) {
  BoundedSequenceEncoding enc(bound_begin, bound_end);
  util::scoped_memory backing(util::MallocOrThrow(enc.EncodedLength() + 7 / 8) * 8);
  std::vector<unsigned char> values(bound_end - bound_begin), out(bound_end - bound_begin);
  while (true) {
    enc.Encode(&values[0], backing.get());
    enc.Decode(backing.get(), &out[0]);
    for (std::size_t i = 0; i != values.size(); ++i) {
      BOOST_CHECK_EQUAL(values[i], out[i]);
    }
    for (std::size_t i = 0; ; ++i) {
      if (i == values.size()) return;
      ++values[i];
      if (values[i] < bound_begin[i]) break;
      values[i] = 0;
    }
  }
}

BOOST_AUTO_TEST_CASE(Exhaustive) {
  unsigned char bounds[] = {5,2,3,9,7,20};
  ExhaustiveTest(bounds, bounds + 6);
}

}} // namespaces
