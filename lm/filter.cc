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

SingleVocabFilter::SingleVocabFilter(std::istream &vocab, const char *out) : SingleOutputFilter(out) {
  std::auto_ptr<std::string> word(new std::string());
  while (vocab >> *word) {
    if (words_.insert(StringPiece(*word)).second) {
      backing_.push_back(word);
      word.reset(new std::string());
    }
  }
  if (!vocab.eof()) err(1, "Reading text from stdin");
}

MultipleVocabMultipleOutputFilter::MultipleVocabMultipleOutputFilter(const Map &vocabs, unsigned int sentence_count, const char *prefix) : vocabs_(vocabs) {
  files_.reserve(sentence_count);
  std::string tmp;
  for (unsigned int i = 0; i < sentence_count; ++i) {
    tmp = prefix;
    tmp += boost::lexical_cast<std::string>(i);
    files_.push_back(new ARPAOutput(tmp.c_str()));
  }
}

} // namespace lm
