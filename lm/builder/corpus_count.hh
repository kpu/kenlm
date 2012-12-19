#ifndef LM_BUILDER_CORPUS_COUNT__
#define LM_BUILDER_CORPUS_COUNT__

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
    CorpusCount(util::FilePiece &from, std::size_t order, int vocab_write)
      : from_(from), order_(order), vocab_write_(vocab_write), token_count_(0) {}

  void Run(const util::stream::ChainPosition &position);

  // How many tokens did we read from the text input (not including BOS/SOS)?
  uint64_t TokenCount() { return token_count_; }

  // How many unique n-grams were observed for the highest order N?
  uint64_t HighOrderCount() { return high_order_count_; }

  private:
    util::FilePiece &from_;
    const std::size_t order_;
    int vocab_write_;
    uint64_t token_count_;
    uint64_t high_order_count_;
};

} // namespace builder
} // namespace lm
#endif // LM_BUILDER_CORPUS_COUNT__
