#ifndef LM_BUILDER_PIPELINE__
#define LM_BUILDER_PIPELINE__

#include "lm/builder/initial_probabilities.hh"
#include "util/stream/config.hh"

#include <string>
#include <cstddef>

namespace util { class FilePiece; }

namespace lm { namespace builder {

struct PipelineConfig {
  std::size_t order;
  std::string vocab_file;
  util::stream::ChainConfig chain;
  util::stream::SortConfig sort;
  InitialProbabilitiesConfig initial_probs;

  const std::string &TempPrefix() const { return sort.temp_prefix; }
};

void Pipeline(const PipelineConfig &config, util::FilePiece &text, std::ostream &out);

}} // namespaces
#endif // LM_BUILDER_PIPELINE__
