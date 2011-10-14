#include "util/tokenize_piece.hh"
#include "util/string_piece.hh"

#define BOOST_TEST_MODULE TokenIteratorTest
#include <boost/test/unit_test.hpp>

#include <iostream>

namespace util {
namespace {

BOOST_AUTO_TEST_CASE(simple) {
  PieceIterator<' '> it("single spaced words.");
  BOOST_REQUIRE(it);
  BOOST_CHECK_EQUAL(StringPiece("single"), *it);
  ++it;
  BOOST_REQUIRE(it);
  BOOST_CHECK_EQUAL(StringPiece("spaced"), *it);
  ++it;
  BOOST_REQUIRE(it);
  BOOST_CHECK_EQUAL(StringPiece("words."), *it);
  ++it;
  BOOST_CHECK(!it);
}

BOOST_AUTO_TEST_CASE(null_delimiter) {
  const char str[] = "\0first\0\0second\0\0\0third\0fourth\0\0\0";
  PieceIterator<'\0'> it(StringPiece(str, sizeof(str) - 1));
  BOOST_REQUIRE(it);
  BOOST_CHECK_EQUAL(StringPiece("first"), *it);
  ++it;
  BOOST_REQUIRE(it);
  BOOST_CHECK_EQUAL(StringPiece("second"), *it);
  ++it;
  BOOST_REQUIRE(it);
  BOOST_CHECK_EQUAL(StringPiece("third"), *it);
  ++it;
  BOOST_REQUIRE(it);
  BOOST_CHECK_EQUAL(StringPiece("fourth"), *it);
  ++it;
  BOOST_CHECK(!it);
}

BOOST_AUTO_TEST_CASE(null_entries) {
  const char str[] = "\0split\0\0 \0me\0 ";
  PieceIterator<' '> it(StringPiece(str, sizeof(str) - 1));
  BOOST_REQUIRE(it);
  const char first[] = "\0split\0\0";
  BOOST_CHECK_EQUAL(StringPiece(first, sizeof(first) - 1), *it);
  ++it;
  BOOST_REQUIRE(it);
  const char second[] = "\0me\0";
  BOOST_CHECK_EQUAL(StringPiece(second, sizeof(second) - 1), *it);
  ++it;
  BOOST_CHECK(!it);
}

/*BOOST_AUTO_TEST_CASE(pipe_pipe_none) {
  const char str[] = "nodelimit at all";
  TokenIter<MultiCharacter> it(str, MultiCharacter("|||"));
  BOOST_REQUIRE(it);
  BOOST_CHECK_EQUAL(StringPiece(str), *it);
  ++it;
  BOOST_CHECK(!it);
}
BOOST_AUTO_TEST_CASE(pipe_pipe_two) {
  const char str[] = "|||";
  TokenIter<MultiCharacter> it(str, MultiCharacter("|||"));
  BOOST_REQUIRE(it);
  BOOST_CHECK_EQUAL(StringPiece(), *it);
  ++it;
  BOOST_REQUIRE(it);
  BOOST_CHECK_EQUAL(StringPiece(), *it);
  ++it;
  BOOST_CHECK(!it);
}

BOOST_AUTO_TEST_CASE(remove_empty) {
  const char str[] = "|||";
  TokenIter<MultiCharacter, true> it(str, MultiCharacter("|||"));
  BOOST_CHECK(!it);
}*/

BOOST_AUTO_TEST_CASE(remove_empty_keep) {
  const char str[] = " |||";
  TokenIter<MultiCharacter, true> it(str, MultiCharacter("|||"));
  BOOST_REQUIRE(it);
  BOOST_CHECK_EQUAL(StringPiece(" "), *it);
  ++it;
  BOOST_CHECK(!it);
}

} // namespace
} // namespace util
