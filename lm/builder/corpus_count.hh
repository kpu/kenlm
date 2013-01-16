#ifndef LM_BUILDER_CORPUS_COUNT__
#define LM_BUILDER_CORPUS_COUNT__

#include "lm/word_index.hh"

#include <cstddef>
#include <string>
#include <stdint.h>

namespace util {
class FilePiece;
namespace stream {
class ChainPosition;
} // namespace stream
} // namespace util

namespace lm {
namespace builder {

class CorpusCount {
  public:
    // Memory usage will be DedupeMultipler(order) * block_size + total_chain_size + unknown vocab_hash_size
    static float DedupeMultiplier(std::size_t order);

    CorpusCount(util::FilePiece &from, int vocab_write, uint64_t &token_count, WordIndex &type_count)
      : from_(from), vocab_write_(vocab_write), token_count_(token_count), type_count_(type_count) {
      token_count_ = 0;
      type_count_ = 0;
    }

    void Run(const util::stream::ChainPosition &position);

  private:
    util::FilePiece &from_;
    int vocab_write_;
    uint64_t &token_count_;
    WordIndex &type_count_;
};

} // namespace builder
} // namespace lm
#endif // LM_BUILDER_CORPUS_COUNT__
