#include "lm/read_arpa.hh"

#include <cstdlib>
#include <ctype.h>

namespace lm {

bool IsEntirelyWhiteSpace(const StringPiece &line) {
  for (size_t i = 0; i < static_cast<size_t>(line.size()); ++i) {
    if (!isspace(line.data()[i])) return false;
  }
  return true;
}

void ReadARPACounts(util::FilePiece &in, std::vector<size_t> &number) {
  number.clear();
  StringPiece line;
  if (!IsEntirelyWhiteSpace(line = in.ReadLine())) UTIL_THROW(FormatLoadException, "First line was \"" << line << "\" not blank");
  if ((line = in.ReadLine()) != "\\data\\") UTIL_THROW(FormatLoadException, "second line was \"" << line << "\" not \\data\\.");
  while (!IsEntirelyWhiteSpace(line = in.ReadLine())) {
    if (line.size() < 6 || strncmp(line.data(), "ngram ", 6)) UTIL_THROW(FormatLoadException, "count line \"" << line << "\"doesn't begin with       \"ngram \"");
    // So strtol doesn't go off the end of line.  
    std::string remaining(line.data() + 6, line.size() - 6);
    char *end_ptr;
    unsigned long int length = std::strtol(remaining.c_str(), &end_ptr, 10);
    if ((end_ptr == remaining.c_str()) || (length - 1 != number.size())) UTIL_THROW(FormatLoadException, "ngram count lengths should be consecutive  starting with 1: " << line);
    if (*end_ptr != '=') UTIL_THROW(FormatLoadException, "Expected = immediately following the first number in the count line " << line);
    ++end_ptr;
    const char *start = end_ptr;
    long int count = std::strtol(start, &end_ptr, 10);
    if (count < 0) UTIL_THROW(FormatLoadException, "Negative n-gram count " << count);
    if (start == end_ptr) UTIL_THROW(FormatLoadException, "Couldn't parse n-gram count from " << line);
    number.push_back(count);
  }
}

void ReadNGramHeader(util::FilePiece &in, unsigned int length) {
  StringPiece line;
  while (IsEntirelyWhiteSpace(line = in.ReadLine())) {}
  std::stringstream expected;
  expected << '\\' << length << "-grams:";
  if (line != expected.str()) UTIL_THROW(FormatLoadException, "Was expecting n-gram header " << expected.str() << " but got " << line << " instead.  ");
}

void ReadBackoff(util::FilePiece &f, Prob &weights) {
  switch (f.get()) {
    case '\t':
      UTIL_THROW(FormatLoadException, "Backoff " << f.ReadDelimited() << " provided for an n-gram that should have no backoff.");
      break;
    case '\n':
      break;
    default:
      UTIL_THROW(FormatLoadException, "Expected tab or newline after unigram");
  }
}

void ReadBackoff(util::FilePiece &f, ProbBackoff &weights) {
  switch (f.get()) {
    case '\t':
      weights.backoff = f.ReadFloat();
      if (weights.backoff > 0) UTIL_THROW(FormatLoadException, "Backoff " << weights.backoff << " > 0");
      if ((f.get() != '\n')) UTIL_THROW(FormatLoadException, "Expected newline after backoff");
      break;
    case '\n':
      weights.backoff = -0.0;
      break;
    default:
      UTIL_THROW(FormatLoadException, "Expected tab or newline after unigram");
  }
}

} // namespace lm
