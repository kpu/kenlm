/* Filter an ARPA language model to only contain words found in a vocabulary
 * plus <s>, </s>, and <unk>.
 */

#include "lm/filter.hh"

#include <istream>
#include <ostream>
#include <memory>
#include <string>

#include <err.h>
#include <string.h>

namespace lm {

SingleBinary::SingleBinary(std::istream &vocab) {
  std::auto_ptr<std::string> word(new std::string());
  while (vocab >> *word) {
    if (words_.insert(StringPiece(*word)).second) {
      backing_.push_back(word);
      word.reset(new std::string());
    }
  }
  if (!vocab.eof()) err(1, "Reading text from stdin");
}

} // namespace lm
