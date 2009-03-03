#include "util/tokenize_piece.hh"

#define BOOST_TEST_MODULE TokenIteratorTest
#include <boost/test/unit_test.hpp>

namespace tokenize {
namespace {

BOOST_AUTO_TEST_CASE(simple) {
	AnyCharacterDelimiter delim(" ");
	PieceIterator it("single spaced words.", delim);
	BOOST_REQUIRE(it != kEndPieceIterator);
	BOOST_CHECK_EQUAL(StringPiece("single"), *it);
	++it;
	BOOST_REQUIRE(it != kEndPieceIterator);
	BOOST_CHECK_EQUAL(StringPiece("spaced"), *it);
	++it;
	BOOST_REQUIRE(it != kEndPieceIterator);
	BOOST_CHECK_EQUAL(StringPiece("words."), *it);
	++it;
	BOOST_CHECK(it == kEndPieceIterator);
}

BOOST_AUTO_TEST_CASE(null_delimiter) {
	AnyCharacterDelimiter delim(StringPiece("\0", 1));
	const char str[] = "\0first\0\0second\0\0\0third\0fourth\0\0\0";
	PieceIterator it(StringPiece(str, sizeof(str) - 1), delim);
	BOOST_REQUIRE(it != kEndPieceIterator);
	BOOST_CHECK_EQUAL(StringPiece("first"), *it);
	++it;
	BOOST_REQUIRE(it != kEndPieceIterator);
	BOOST_CHECK_EQUAL(StringPiece("second"), *it);
	++it;
	BOOST_REQUIRE(it != kEndPieceIterator);
	BOOST_CHECK_EQUAL(StringPiece("third"), *it);
	++it;
	BOOST_REQUIRE(it != kEndPieceIterator);
	BOOST_CHECK_EQUAL(StringPiece("fourth"), *it);
	++it;
	BOOST_CHECK(it == kEndPieceIterator);
}

BOOST_AUTO_TEST_CASE(null_entries) {
	AnyCharacterDelimiter delim(" ");
	const char str[] = "\0split\0\0 \0me\0 ";
	PieceIterator it(StringPiece(str, sizeof(str) - 1), delim);
	BOOST_REQUIRE(it != kEndPieceIterator);
	const char first[] = "\0split\0\0";
	BOOST_CHECK_EQUAL(StringPiece(first, sizeof(first) - 1), *it);
	++it;
	BOOST_REQUIRE(it != kEndPieceIterator);
	const char second[] = "\0me\0";
	BOOST_CHECK_EQUAL(StringPiece(second, sizeof(second) - 1), *it);
	++it;
	BOOST_CHECK(it == kEndPieceIterator);
}

BOOST_AUTO_TEST_CASE(strange) {
	AnyCharacterDelimiter delim("Qoz");
	PieceIterator it("nm eQo z10zzzzA!oooz", delim);
	BOOST_REQUIRE(it != kEndPieceIterator);
	BOOST_CHECK_EQUAL(StringPiece("nm e"), *it);
	++it;
	BOOST_REQUIRE(it != kEndPieceIterator);
	BOOST_CHECK_EQUAL(StringPiece(" "), *it);
	++it;
	BOOST_REQUIRE(it != kEndPieceIterator);
	BOOST_CHECK_EQUAL(StringPiece("10"), *it);
	++it;
	BOOST_REQUIRE(it != kEndPieceIterator);
	BOOST_CHECK_EQUAL(StringPiece("A!"), *it);
	++it;
	BOOST_CHECK(it == kEndPieceIterator);
}

} // namespace
} // namespace tokenize
