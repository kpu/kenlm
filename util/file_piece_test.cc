#include "util/file_piece.hh"

#define BOOST_TEST_MODULE FilePieceTest
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iostream>

namespace util {
namespace {

BOOST_AUTO_TEST_CASE(ReadLine) {
  std::fstream ref("file_piece.hh", std::ios::in);
  FilePiece test("file_piece.hh", 1);
  std::string ref_line;
  while (getline(ref, ref_line)) {
    StringPiece test_line(test.ReadLine());
    if (test_line != ref_line) {
      std::cerr << test_line.size() << " " << ref_line.size() << std::endl;
    }
    BOOST_CHECK_EQUAL(ref_line, test_line);
  }
}

} // namespace
} // namespace util
