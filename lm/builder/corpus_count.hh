#ifndef LM_BUILDER_CORPUS_COUNT__
#define LM_BUILDER_CORPUS_COUNT__

#include "lm/word_index.hh"
#include "util/scoped.hh"

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

    // How much memory vocabulary will use based on estimated size of the vocab.
    static std::size_t VocabUsage(std::size_t vocab_estimate);

    // token_count: out.
    // type_count aka vocabulary size.  Initialize to an estimate.  It is set to the exact value.
    CorpusCount(util::FilePiece &from, int vocab_write, uint64_t &token_count, WordIndex &type_count, std::size_t entries_per_block);

    void Run(const util::stream::ChainPosition &position);

  private:
    util::FilePiece &from_;
    int vocab_write_;
    uint64_t &token_count_;
    WordIndex &type_count_;

    std::size_t dedupe_mem_size_;
    util::scoped_malloc dedupe_mem_;
};

} // namespace builder
} // namespace lm
#endif // LM_BUILDER_CORPUS_COUNT__
