#ifndef LM_BUILDER_PIPELINE__
#define LM_BUILDER_PIPELINE__

#include <string>
#include <cstddef>

namespace lm { namespace builder {

struct PipelineConfig {
  std::size_t order;
  std::string vocab_file;
  util::stream::ChainConfig chain;
  util::stream::SortConfig merge;

  const std::string &TempPrefix() const { return merge.temp_prefix; }
};

void Pipeline(const PipelineConfig &config, util::FilePiece &text, std::ostream &out);

}} // namespaces
#endif // LM_BUILDER_PIPELINE__
