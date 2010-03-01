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
  err(2, "Reading input lm");
}

void ReadNGramHeader(std::istream &in, unsigned int length) {
  std::string line;
  do {
    if (!getline(in, line)) err(2, "Reading from input lm");
  } while (IsEntirelyWhiteSpace(line));
  if (line != (std::string("\\") + boost::lexical_cast<std::string>(length) + "-grams:"))
    errx(3, "Wrong ngram line: %s", line.c_str());
}

void ReadEnd(std::istream &in_lm) {
  std::string line;
  if (!getline(in_lm, line)) errx(2, "Reading \\end\\ from input lm failed.");
  if (line != "\\end\\") errx(3, "Bad end \"%s\"; should be \\end\\", line.c_str());
}

ARPAOutput::ARPAOutput(const char *name)  {
  file_.exceptions(std::ostream::eofbit | std::ostream::failbit | std::ostream::badbit);
  file_.open(name, std::ios::out);
}

void ARPAOutput::ReserveForCounts(std::streampos reserve) {
  for (std::streampos i = 0; i < reserve; i += std::streampos(1)) {
    file_ << '\n';
  }
}

void ARPAOutput::BeginLength(unsigned int length) {
  fast_counter_ = 0;
  file_ << '\\' << length << "-grams:" << '\n';
}

void ARPAOutput::EndLength(unsigned int length) {
  file_ << '\n';
  if (length > counts_.size()) {
    counts_.resize(length);
  }
  counts_[length - 1] = fast_counter_;
}

void ARPAOutput::Finish() {
  file_ << "\\end\\\n";

  file_.seekp(0);
  WriteCounts(file_, counts_);
  file_ << std::flush;
}

} // namespace lm
