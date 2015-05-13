/* Map vocab ids.  This is useful to merge independently collected counts or
 * change the vocab ids to the order used by the trie.
 */
#ifndef LM_BUILDER_RENUMBER__
#define LM_BUILDER_RENUMBER__

#include "lm/word_index.hh"

namespace util { namespace stream { class ChainPosition; }}

namespace lm { namespace builder {

class Renumber {
  public:
    // Assumes the array is large enough to map all words and stays alive while
    // the thread is active.
    explicit Renumber(const WordIndex *new_numbers)
      : new_numbers_(new_numbers) {}

    void Run(const util::stream::ChainPosition &position);

  private:
    const WordIndex *new_numbers_;
};

}} // namespaces
#endif // LM_BUILDER_RENUMBER__
