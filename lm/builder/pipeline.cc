#include "lm/builder/adjust_counts.hh"
#include "lm/builder/corpus_count.hh"

#include <vector>

namespace lm { namespace builder {

void Pipeline(const PipelineConfig &config, util::FilePiece &text, std::ostream &out) {
  util::TempMaker temp(config.temp_prefix);
  std::vector<util::stream::ChainConfig> chain_configs(config.order, config.chain);
  for (std::size_t i = 0; i < config.order; ++i) {
    chain_configs[i].entry_size = NGram::TotalSize(i + 1);
  }

  util::stream::Sort<SuffixOrder, AddCombiner> first_suffix(temp, SuffixOrder(config.order));
  util::stream::Chain(chain_configs.back()) >> CorpusCount(text, config.order, config.vocab_file) >> first_suffix.Unsorted();

  std::vector<uint64_t> counts;
  std::vector<Discount> discounts;
  util::stream::Sort<ContextOrder> second_context(temp, ContextOrder(config.order));
  {
    util::stream::Chains chains(chain_configs);
    chains[config.order - 1] >> first_suffix.Sorted(config.merge);
    chains >> AdjustCounts(counts, discounts) >> second_context;
  }
}

}} // namespaces
