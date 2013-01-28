#ifndef LM_BUILDER_PIPELINE__
#define LM_BUILDER_PIPELINE__

#include "lm/builder/initial_probabilities.hh"
#include "lm/builder/header_info.hh"
#include "lm/word_index.hh"
#include "util/stream/config.hh"
#include "util/file_piece.hh"

#include <string>
#include <cstddef>

namespace lm { namespace builder {

struct PipelineConfig {
  std::size_t order;
  std::string vocab_file;
  util::stream::SortConfig sort;
  InitialProbabilitiesConfig initial_probs;
  util::stream::ChainConfig read_backoffs;
  bool verbose_header;

  // Estimated vocabulary size.  Used for sizing CorpusCount memory and
  // initial probing hash table sizing, also in CorpusCount.
  lm::WordIndex vocab_estimate;

  // Minimum block size to tolerate.
  std::size_t minimum_block;

  // Number of blocks to use.  This will be overridden to 1 if everything fits.
  std::size_t block_count;

  const std::string &TempPrefix() const { return sort.temp_prefix; }
  std::size_t TotalMemory() const { return sort.total_memory; }
};

// Takes ownership of text_file and out_arpa.
void Pipeline(PipelineConfig config, int text_file, int out_arpa);

}} // namespaces
#endif // LM_BUILDER_PIPELINE__
