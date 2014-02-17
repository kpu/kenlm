#ifndef LM_BUILDER_CORPUS_COUNT__
#define LM_BUILDER_CORPUS_COUNT__

#include "lm/lm_exception.hh"
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
    //
    // If dynamic_vocab is true, then vocab ids are created on the fly and the
    // words are written to vocab_file.  If dynamic_vocab is false, then
    // vocab_file is expected to contain an 8-byte count followed by a probing
    // hash table with precomputed vocab ids.
    CorpusCount(util::FilePiece &from, int vocab_file, uint64_t &token_count, WordIndex &type_count, std::size_t entries_per_block, WarningAction disallowed_symbol, bool dynamic_vocab = true);

    void Run(const util::stream::ChainPosition &position);

  private:
    template <class Voc> void RunWithVocab(const util::stream::ChainPosition &position, Voc &vocab);

    util::FilePiece &from_;
    int vocab_file_;
    uint64_t &token_count_;
    WordIndex &type_count_;

    std::size_t dedupe_mem_size_;
    util::scoped_malloc dedupe_mem_;

    WarningAction disallowed_symbol_action_;

    bool dynamic_vocab_;
};

} // namespace builder
} // namespace lm
#endif // LM_BUILDER_CORPUS_COUNT__
