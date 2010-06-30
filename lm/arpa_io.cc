/*  Input and output for ARPA format files.
 */

#include "lm/arpa_io.hh"

#include <boost/lexical_cast.hpp>

#include <istream>
#include <ostream>
#include <string>
#include <vector>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <string.h>

namespace lm {

// Seeking is the responsibility of the caller.
void WriteCounts(std::ostream &out, const std::vector<size_t> &number) {
  out << "\n\\data\\\n";
  for (unsigned int i = 0; i < number.size(); ++i) {
    out << "ngram " << i+1 << "=" << number[i] << '\n';
  }
  out << '\n';
}

size_t SizeNeededForCounts(const std::vector<size_t> &number) {
  std::ostringstream buf;
  WriteCounts(buf, number);
  return buf.tellp();
}

bool IsEntirelyWhiteSpace(const std::string &line) {
  for (size_t i = 0; i < line.size(); ++i) {
    if (!isspace(line[i])) return false;
  }
  return true;
}

void ReadCounts(std::istream &in, std::vector<size_t> &number) {
  number.clear();
  std::string line;
  if (!getline(in, line)) err(2, "Reading input lm");
  if (!IsEntirelyWhiteSpace(line)) errx(3, "First line was \"%s\", not blank.", line.c_str());
  if (!getline(in, line)) err(2, "Reading \\data\\");
  if (!(line == "\\data\\")) err(3, "Second line was \"%s\", not blank.", line.c_str());
  while (getline(in, line)) {
    if (IsEntirelyWhiteSpace(line)) {
      return;
    }
    if (strncmp(line.c_str(), "ngram ", 6))
      errx(3, "data line \"%s\" doesn't begin with \"ngram \"", line.c_str());
    size_t equals = line.find('=');
    if (equals == std::string::npos)
      errx(3, "no equals in \"%s\".", line.c_str());
    unsigned int length = boost::lexical_cast<unsigned int>(line.substr(6, equals - 6));
    if (length - 1 != number.size()) errx(3, "ngram length %i is not expected %i in line %s", length, static_cast<unsigned int>(number.size() + 1), line.c_str());
    unsigned int count = boost::lexical_cast<unsigned int>(line.substr(equals + 1));
    number.push_back(count);
  }
  errx(2, "Reading counts from input lm failed");
}

void ReadNGramHeader(std::istream &in, unsigned int length) {
  std::string line;
  do {
    if (!getline(in, line)) errx(2, "Reading header for n-gram length %i from input lm failed", length);
  } while (IsEntirelyWhiteSpace(line));
  if (line != (std::string("\\") + boost::lexical_cast<std::string>(length) + "-grams:"))
    errx(3, "Wrong ngram line: %s", line.c_str());
}

void ReadEnd(std::istream &in_lm) {
  std::string line;
  do {
    if (!getline(in_lm, line)) errx(2, "Reading end marker failed");
  } while (IsEntirelyWhiteSpace(line));
  if (line != "\\end\\") errx(3, "Bad end \"%s\"; should be \\end\\", line.c_str());
}

ARPAOutputException::ARPAOutputException(const char *message, const std::string &file_name) throw()
  : what_(std::string(message) + " file " + file_name), file_name_(file_name) {
  if (errno) {
    char buf[1024];
    buf[0] = 0;
#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! _GNU_SOURCE
    const char *add = buf;
    if (!strerror_r(errno, buf, 1024)) {
#else
    const char *add = strerror_r(errno, buf, 1024);
    if (add) {
#endif
      what_ += " :";
      what_ += add;
    }
  }
}

ARPAOutput::ARPAOutput(const char *name) : file_name_(name), file_(name, std::ios::out) {
  try {
    file_.exceptions(std::ostream::eofbit | std::ostream::failbit | std::ostream::badbit);
  } catch (const std::ios_base::failure &f) {
    throw ARPAOutputException("Opening", file_name_);
  }
}

void ARPAOutput::ReserveForCounts(std::streampos reserve) {
  try {
    for (std::streampos i = 0; i < reserve; i += std::streampos(1)) {
      file_ << '\n';
    }
  } catch (const std::ios_base::failure &f) {
    throw ARPAOutputException("Writing blanks to reserve space for counts to ", file_name_);
  }
}

void ARPAOutput::BeginLength(unsigned int length) {
  fast_counter_ = 0;
  try {
    file_ << '\\' << length << "-grams:" << '\n';
  } catch (const std::ios_base::failure &f) {
    throw ARPAOutputException("Writing n-gram header to ", file_name_);
  }
}

void ARPAOutput::EndLength(unsigned int length) {
  try {
    file_ << '\n';
  } catch (const std::ios_base::failure &f) {
    throw ARPAOutputException("Writing blank at end of count list to ", file_name_);
  }
  if (length > counts_.size()) {
    counts_.resize(length);
  }
  counts_[length - 1] = fast_counter_;
}

void ARPAOutput::Finish() {
  try {
    file_ << "\\end\\\n";
    file_.seekp(0);
    WriteCounts(file_, counts_);
    file_ << std::flush;
  } catch (const std::ios_base::failure &f) {
    throw ARPAOutputException("Finishing including writing counts at beginning to ", file_name_);
  }
}

} // namespace lm
