#include "util/file_piece.hh"

#include "util/scoped.hh"

#define BOOST_TEST_MODULE FilePieceTest
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <iostream>

#include <stdio.h>

namespace util {
namespace {

/* mmap implementation */
BOOST_AUTO_TEST_CASE(MMapReadLine) {
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
  BOOST_CHECK_THROW(test.get(), EndOfFileException);
}

/* read() implementation */
BOOST_AUTO_TEST_CASE(StreamReadLine) {
  std::fstream ref("file_piece.cc", std::ios::in);

  scoped_FILE catter(popen("cat file_piece.cc", "r"));
  BOOST_REQUIRE(catter.get());
  
  FilePiece test(dup(fileno(catter.get())), "file_piece.cc", NULL, 1);
  std::string ref_line;
  while (getline(ref, ref_line)) {
    StringPiece test_line(test.ReadLine());
    // I submitted a bug report to ICU: http://bugs.icu-project.org/trac/ticket/7924
    if (!test_line.empty() || !ref_line.empty()) {
      BOOST_CHECK_EQUAL(ref_line, test_line);
    }
  }
  BOOST_CHECK_THROW(test.get(), EndOfFileException);
}

#ifdef HAVE_ZLIB

// gzip file
BOOST_AUTO_TEST_CASE(PlainZipReadLine) {
  std::fstream ref("file_piece.cc", std::ios::in);

  BOOST_REQUIRE_EQUAL(0, system("gzip <file_piece.cc >file_piece.cc.gz"));
  FilePiece test("file_piece.cc.gz", NULL, 1);
  std::string ref_line;
  while (getline(ref, ref_line)) {
    StringPiece test_line(test.ReadLine());
    // I submitted a bug report to ICU: http://bugs.icu-project.org/trac/ticket/7924
    if (!test_line.empty() || !ref_line.empty()) {
      BOOST_CHECK_EQUAL(ref_line, test_line);
    }
  }
  BOOST_CHECK_THROW(test.get(), EndOfFileException);
}
// gzip stream
BOOST_AUTO_TEST_CASE(StreamZipReadLine) {
  std::fstream ref("file_piece.cc", std::ios::in);

  scoped_FILE catter(popen("gzip <file_piece.cc", "r"));
  BOOST_REQUIRE(catter.get());
  
  FilePiece test(dup(fileno(catter.get())), "file_piece.cc", NULL, 1);
  std::string ref_line;
  while (getline(ref, ref_line)) {
    StringPiece test_line(test.ReadLine());
    // I submitted a bug report to ICU: http://bugs.icu-project.org/trac/ticket/7924
    if (!test_line.empty() || !ref_line.empty()) {
      BOOST_CHECK_EQUAL(ref_line, test_line);
    }
  }
  BOOST_CHECK_THROW(test.get(), EndOfFileException);
}

#endif

} // namespace
} // namespace util
