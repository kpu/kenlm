#include "util/file_piece.hh"

#define BOOST_TEST_MODULE FilePieceTest
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iostream>

namespace util {
namespace {

/* mmap implementation */
BOOST_AUTO_TEST_CASE(MMapLine) {
  std::fstream ref("file_piece.cc", std::ios::in);
  FilePiece test("file_piece.cc", NULL, 1);
  std::string ref_line;
  while (getline(ref, ref_line)) {
    StringPiece test_line(test.ReadLine());
    // I submitted a bug report to ICU: http://bugs.icu-project.org/trac/ticket/7924
    if (!test_line.empty() || !ref_line.empty()) {
      BOOST_CHECK_EQUAL(ref_line, test_line);
    }
  }
}

/* read() implementation */
BOOST_AUTO_TEST_CASE(ReadLine) {
  std::fstream ref("file_piece.cc", std::ios::in);
  FilePiece test("file_piece.cc", NULL, 1);
  test.ForceFallbackToRead();
  std::string ref_line;
  while (getline(ref, ref_line)) {
    StringPiece test_line(test.ReadLine());
    // I submitted a bug report to ICU: http://bugs.icu-project.org/trac/ticket/7924
    if (!test_line.empty() || !ref_line.empty()) {
      BOOST_CHECK_EQUAL(ref_line, test_line);
    }
  }
}

} // namespace
} // namespace util
