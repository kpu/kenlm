#include "lm/read_arpa.hh"

#include <cstdlib>
#include <vector>

#include <ctype.h>
#include <inttypes.h>

namespace lm {

namespace {

bool IsEntirelyWhiteSpace(const StringPiece &line) {
  for (size_t i = 0; i < static_cast<size_t>(line.size()); ++i) {
    if (!isspace(line.data()[i])) return false;
  }
  return true;
}

template <class F> void GenericReadARPACounts(F &in, std::vector<uint64_t> &number) {
  number.clear();
  StringPiece line;
  if (!IsEntirelyWhiteSpace(line = in.ReadLine())) UTIL_THROW(FormatLoadException, "First line was \"" << line << "\" not blank");
  if ((line = in.ReadLine()) != "\\data\\") UTIL_THROW(FormatLoadException, "second line was \"" << line << "\" not \\data\\.");
  while (!IsEntirelyWhiteSpace(line = in.ReadLine())) {
    if (line.size() < 6 || strncmp(line.data(), "ngram ", 6)) UTIL_THROW(FormatLoadException, "count line \"" << line << "\"doesn't begin with \"ngram \"");
    // So strtol doesn't go off the end of line.  
    std::string remaining(line.data() + 6, line.size() - 6);
    char *end_ptr;
    unsigned long int length = std::strtol(remaining.c_str(), &end_ptr, 10);
    if ((end_ptr == remaining.c_str()) || (length - 1 != number.size())) UTIL_THROW(FormatLoadException, "ngram count lengths should be consecutive starting with 1: " << line);
    if (*end_ptr != '=') UTIL_THROW(FormatLoadException, "Expected = immediately following the first number in the count line " << line);
    ++end_ptr;
    const char *start = end_ptr;
    long int count = std::strtol(start, &end_ptr, 10);
    if (count < 0) UTIL_THROW(FormatLoadException, "Negative n-gram count " << count);
    if (start == end_ptr) UTIL_THROW(FormatLoadException, "Couldn't parse n-gram count from " << line);
    number.push_back(count);
  }
}

template <class F> void GenericReadNGramHeader(F &in, unsigned int length) {
  StringPiece line;
  while (IsEntirelyWhiteSpace(line = in.ReadLine())) {}
  std::stringstream expected;
  expected << '\\' << length << "-grams:";
  if (line != expected.str()) UTIL_THROW(FormatLoadException, "Was expecting n-gram header " << expected.str() << " but got " << line << " instead.  ");
}

template <class F> void GenericReadEnd(F &in) {
  StringPiece line;
  do {
    line = in.ReadLine();
  } while (IsEntirelyWhiteSpace(line));
  if (line != "\\end\\") UTIL_THROW(FormatLoadException, "Expected \\end\\ but the ARPA file has " << line);
}

class FakeFilePiece {
  public:
    explicit FakeFilePiece(std::istream &in) : in_(in) {
      in_.exceptions(std::ios::failbit | std::ios::badbit | std::ios::eofbit);
    }

    StringPiece ReadLine() throw(util::EndOfFileException) {
      getline(in_, buffer_);
      return StringPiece(buffer_);
    }

    float ReadFloat() {
      float ret;
      in_ >> ret;
      return ret;
    }

  private:
    std::istream &in_;
    std::string buffer_;
};

} // namespace

void ReadARPACounts(util::FilePiece &in, std::vector<uint64_t> &number) {
  GenericReadARPACounts(in, number);
}
void ReadARPACounts(std::istream &in, std::vector<uint64_t> &number) {
  FakeFilePiece fake(in);
  GenericReadARPACounts(fake, number);
}
void ReadNGramHeader(util::FilePiece &in, unsigned int length) {
  GenericReadNGramHeader(in, length);
}
void ReadNGramHeader(std::istream &in, unsigned int length) {
  FakeFilePiece fake(in);
  GenericReadNGramHeader(fake, length);
}

void ReadBackoff(util::FilePiece &in, Prob &/*weights*/) {
  switch (in.get()) {
    case '\t':
      {
        float got = in.ReadFloat();
        if (got != 0.0)
          UTIL_THROW(FormatLoadException, "Non-zero backoff " << got << " provided for an n-gram that should have no backoff.");
      }
      break;
    case '\n':
      break;
    default:
      UTIL_THROW(FormatLoadException, "Expected tab or newline after unigram");
  }
}

void ReadBackoff(util::FilePiece &in, ProbBackoff &weights) {
  switch (in.get()) {
    case '\t':
      weights.backoff = in.ReadFloat();
      if ((in.get() != '\n')) UTIL_THROW(FormatLoadException, "Expected newline after backoff");
      break;
    case '\n':
      weights.backoff = 0.0;
      break;
    default:
      UTIL_THROW(FormatLoadException, "Expected tab or newline after unigram");
  }
}

void ReadEnd(util::FilePiece &in) {
  GenericReadEnd(in);
}
void ReadEnd(std::istream &in) {
  FakeFilePiece fake(in);
  GenericReadEnd(fake);
}

} // namespace lm
