#include "lm/builder/pipeline.hh"

#include "lm/builder/adjust_counts.hh"
#include "lm/builder/corpus_count.hh"
#include "lm/builder/print.hh"
#include "lm/builder/sort.hh"
#include "lm/builder/uninterpolated.hh"

#include "util/file.hh"

#include <vector>

namespace lm { namespace builder {

class UninterpolatedArray : public FixedArray<Uninterpolated> {
  public:
    explicit UninterpolatedArray(std::size_t count) : FixedArray<Uninterpolated>(count) {}

    void push_back(int input_file, const util::stream::ChainConfig &adder_in, const util::stream::ChainConfig &adder_out, const Discount &discount) {
      new (end()) Uninterpolated(input_file, adder_in, adder_out, discount);
    }
};

void Pipeline(const PipelineConfig &config, util::FilePiece &text, std::ostream &out) {
  util::scoped_fd vocab_file(config.vocab_file.empty() ? 
      util::MakeTemp(config.TempPrefix()) : 
      util::CreateOrThrow(config.vocab_file.c_str()));
  std::vector<util::stream::ChainConfig> chain_configs(config.order, config.chain);
  for (std::size_t i = 0; i < config.order; ++i) {
    chain_configs[i].entry_size = NGram::TotalSize(i + 1);
  }
  Chains chains(chain_configs);

  chains[config.order - 1] >> CorpusCount(text, config.order, vocab_file.get());
  BlockingSort(chains[config.order - 1], config.sort, SuffixOrder(config.order), AddCombiner());

  std::cerr << "Adjusting" << std::endl;
  std::vector<uint64_t> counts;
  std::vector<Discount> discounts;
  chains >> AdjustCounts(counts, discounts);

  {
    Sorts<ContextOrder> sorts(chains, config.sort);
    chains.Wait(true);

    std::cerr << "Counts are:\n";
    for (size_t i = 0; i < chains.size(); ++i)
      std::cerr << i << " " << counts[i] << std::endl;

    util::stream::ChainConfig read_ahead;
    read_ahead.block_size = 512;
    read_ahead.block_count = 2;
    read_ahead.queue_length = 2;
    UninterpolatedArray uninterp(chains.size());
    for (size_t i = 0; i < chains.size(); ++i) {
      uninterp.push_back(sorts[i].StealCompleted(), chain_configs[i], read_ahead, discounts[i]);
      chains[i] >> util::stream::PRead(uninterp[i].Input()) >> boost::ref(uninterp[i]);
    }
    BlockingSort<SuffixOrder>(chains, config.sort);
  }

  std::cerr << "Printing" << std::endl;
  VocabReconstitute vocab(vocab_file.get());
  chains >> Print<Uninterp>(vocab, out) >> util::stream::kRecycle;
  chains.Wait();
}

}} // namespaces
