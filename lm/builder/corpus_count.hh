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
    CorpusCount(util::FilePiece &from, int vocab_write, uint64_t &token_count)
      : from_(from), vocab_write_(vocab_write), token_count_(token_count) {
      token_count_ = 0;
    }

    void Run(const util::stream::ChainPosition &position);

  private:
    util::FilePiece &from_;
    int vocab_write_;
    uint64_t &token_count_;
};

} // namespace builder
} // namespace lm
#endif // LM_BUILDER_CORPUS_COUNT__
