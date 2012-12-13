#include "lm/builder/pipeline.hh"

#include "lm/builder/adjust_counts.hh"
#include "lm/builder/corpus_count.hh"
#include "lm/builder/print.hh"
#include "lm/builder/sort.hh"

#include "util/file.hh"

#include <vector>

namespace lm { namespace builder {

void Pipeline(const PipelineConfig &config, util::FilePiece &text, std::ostream &out) {
  std::vector<util::stream::ChainConfig> chain_configs(config.order, config.chain);
  for (std::size_t i = 0; i < config.order; ++i) {
    chain_configs[i].entry_size = NGram::TotalSize(i + 1);
  }

  util::scoped_FILE vocab_file(config.vocab_file.empty() ? 
      util::FMakeTemp(config.TempPrefix()) : 
      util::FOpenOrThrow(config.vocab_file.c_str()));

  util::stream::Sort<SuffixOrder, AddCombiner> first_suffix(config.sort, SuffixOrder(config.order));
  util::stream::Chain(chain_configs.back()) >> CorpusCount(text, config.order, vocab_file.get()) >> first_suffix.Unsorted();

  std::cerr << "Finished counting" << std::endl;

  std::vector<uint64_t> counts;
  std::vector<Discount> discounts;
  Sorts<ContextOrder> second_context(config.sort);
  {
    Chains chains(chain_configs);
    chains[config.order - 1] >> first_suffix.Sorted();
    chains >> AdjustCounts(counts, discounts) >> second_context.Unsorted();
  }
  std::cerr << "Finished adjusting" << std::endl;
  {
    VocabReconstitute vocab(fileno(vocab_file.get()));
    Chains(chain_configs) >> second_context.Sorted() >> Print<uint64_t>(vocab, out) >> util::stream::kRecycle;
  }
}

}} // namespaces
