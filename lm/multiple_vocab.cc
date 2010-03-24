/* Filter an ARPA language model to only contain words found in a vocabulary
 * plus <s>, </s>, and <unk>.
 */

#include "lm/multiple_vocab.hh"

#include <istream>
#include <iostream>

#include <ctype.h>
#include <err.h>

namespace lm {

bool IsLineEnd(std::istream &in) {
  int got;
  do {
    got = in.get();
    if (!in) return true;
    if (got == '\n') return true;
  } while (isspace(got));
  in.unget();
  return false;
}

// Read space separated words in enter separated lines.  
void ReadMultipleVocab(std::istream &in, PrepareMultipleVocab &out) {
  while (in >> out.TempStr()) {
    out.Insert();
    if (IsLineEnd(in)) {
      out.EndSentence();
    }
  }
  if (!in.eof()) {
    err(2, "Reading vocabulary");
  }
}

} // namespace lm
