#ifndef LM_ENUMERATE_VOCAB__
#define LM_ENUMERATE_VOCAB__

#include "lm/word_index.hh"
#include "util/string_piece.hh"

namespace lm {
namespace ngram {

/* If you need the actual strings in the vocabulary, inherit from this class
 * and implement Add.  Then put a pointer in Config.enumerate_vocab.  
 * Add is called once per n-gram.  index starts at 0 and increases by 1 each
 * time.  
 */
class EnumerateVocab {
  public:
    virtual ~EnumerateVocab() {}

    virtual void Add(WordIndex index, const StringPiece &str) = 0;

  protected:
    EnumerateVocab() {}
};

} // namespace ngram
} // namespace lm

#endif // LM_ENUMERATE_VOCAB__

