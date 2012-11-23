#ifndef LM_BUILDER_CORPUS_COUNT__
#define LM_BUILDER_CORPUS_COUNT__

#include <cstddef>

namespace util {
class FilePiece;
namespace stream {
class ChainPosition;
} // namespace stream
} // namespace util

namespace lm {
namespace builder {

void CorpusCount(util::FilePiece &from, std::size_t order, const util::stream::ChainPosition &position, const char *vocab_write);

} // namespace builder
} // namespace lm
#endif // LM_BUILDER_CORPUS_COUNT__
