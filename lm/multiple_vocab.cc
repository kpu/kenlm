/* Filter an ARPA language model to only contain words found in a vocabulary
 * plus <s>, </s>, and <unk>.
 */

#include "lm/multiple_vocab.hh"

#include <istream>

#include <err.h>

namespace lm {

// Read space separated words in enter separated lines.  
void ReadMultipleVocab(std::istream &in, PrepareMultipleVocab &out) {
  while (true) {
    bool empty = true;
    while (in >> out.TempStr()) {
      out.Insert();
      empty = false;
      int got = in.get();
      if (!in) break;
      if (got == '\n') break;
    }
    if (!in) {
      if (!empty) out.EndSentence();
      break;
    }
    out.EndSentence();
  }
  if (!in.eof()) {
    err(2, "Reading vocabulary");
  }
}

} // namespace lm
