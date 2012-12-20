#ifndef LM_BUILDER_PIPELINE__
#define LM_BUILDER_PIPELINE__

#include "lm/builder/initial_probabilities.hh"
#include "lm/builder/header_info.hh"
#include "util/stream/config.hh"
#include "util/file_piece.hh"

#include <string>
#include <cstddef>

namespace lm { namespace builder {

struct PipelineConfig {
  std::size_t order;
  std::string vocab_file;
  util::stream::ChainConfig chain;
  util::stream::SortConfig sort;
  InitialProbabilitiesConfig initial_probs;
  util::stream::ChainConfig read_backoffs;
  bool verbose_header;
  bool sorted_arpa;

  const std::string &TempPrefix() const { return sort.temp_prefix; }
};

void Pipeline(PipelineConfig config, util::FilePiece &text, std::ostream &out);

}} // namespaces
#endif // LM_BUILDER_PIPELINE__
