#include "util/bit_packing.hh"
#include "lm/interpolate/bounded_sequence_encoding.hh"
#include "lm/interpolate/bounded_sequence_iterator.hh"

#define BOOST_TEST_MODULE BoundedSequenceEncodingTest
#include <boost/test/unit_test.hpp>

#include <cstring>
#include <iostream>


namespace lm {
namespace interpolate {

BOOST_AUTO_TEST_CASE(BoundedSequenceIteratorEncoderTest) {
  unsigned char numbers[] = {1,1,2,3,5,8};
  int count = 0;
  BoundedSequenceIteratorEncoder it_from(numbers);
  BoundedSequenceIteratorEncoder it_until(numbers+6);
  for (BoundedSequenceIteratorEncoder it=it_from; it !=it_until; it++) {
    //std::cout << *it << ' ';
    BOOST_CHECK_EQUAL(*it, numbers[count]);
    count++;
  }
  //std::endl;
}

BOOST_AUTO_TEST_CASE(BoundedSequenceIteratorDecoderTest) {
  uint8_t numbers[] = {1,1,2,3,5,8};
  int count = 0;
  BoundedSequenceIteratorDecoder it_from(numbers);
  BoundedSequenceIteratorDecoder it_until(numbers+6);
  for (BoundedSequenceIteratorDecoder it=it_from; it !=it_until; it++) {
    //std::cout << *it << ' ';
    BOOST_CHECK_EQUAL(*it, numbers[count]);
    count++;
  }
  //std::endl;
}

}} // namespaces
