#ifndef LM_BUILDER_CORPUS_COUNT__
#define LM_BUILDER_CORPUS_COUNT__

#include <cstddef>
#include <string>

#include <stdio.h>

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
    CorpusCount(util::FilePiece &from, std::size_t order, FILE *vocab_write)
      : from_(from), order_(order), vocab_write_(vocab_write) {}

    void Run(const util::stream::ChainPosition &position);

  private:
    util::FilePiece &from_;
    const std::size_t order_;
    FILE *vocab_write_;
};

} // namespace builder
} // namespace lm
#endif // LM_BUILDER_CORPUS_COUNT__
