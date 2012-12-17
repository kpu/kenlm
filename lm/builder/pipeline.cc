#include "lm/builder/pipeline.hh"

#include "lm/builder/adjust_counts.hh"
#include "lm/builder/backoff.hh"
#include "lm/builder/corpus_count.hh"
#include "lm/builder/initial_probabilities.hh"
#include "lm/builder/interpolate.hh"
#include "lm/builder/print.hh"
#include "lm/builder/sort.hh"

#include "util/file.hh"

#include <iostream>
#include <vector>

namespace lm { namespace builder {

namespace {
void PrintStatistics(const std::vector<uint64_t> &counts, const std::vector<Discount> &discounts) {
  std::cerr << "Statistics:\n";
  for (size_t i = 0; i < counts.size(); ++i) {
    std::cerr << (i + 1) << ' ' << counts[i];
    for (size_t d = 1; d <= 3; ++d)
      std::cerr << " D" << d << (d == 3 ? "+=" : "=") << discounts[i].amount[d];
    std::cerr << '\n';
  }
}
} // namespace

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
  std::cerr << "Adjusted counts" << std::endl;
  std::vector<uint64_t> counts;
  std::vector<Discount> discounts;
  chains >> AdjustCounts(counts, discounts);

  {
    Sorts<ContextOrder> sorts(chains, config.sort);
    chains.Wait(true);
    PrintStatistics(counts, discounts);

    std::cerr << "Uninterpolated probabilities" << std::endl;
    // Short leashes to minimize the distance between the adder's position and the rejoiner's position.   
    util::stream::ChainConfig adder_in, adder_out;
    adder_in.block_size = 32768;
    adder_in.block_count = 2;
    adder_out.block_size = 512;
    adder_out.block_count = 2;
    for (size_t i = 0; i < chains.size(); ++i) {
      util::scoped_fd fd(sorts[i].StealCompleted());
      chains[i] >> util::stream::PRead(fd.get());
      chains[i] >> InitialProbabilities(fd.release(), adder_in, adder_out, discounts[i]);
    }
  }
  BlockingSort<SuffixOrder>(chains, config.sort);
  std::cerr << "Interpolated probabilities" << std::endl;
  chains >> Interpolate(counts[0]);

  BlockingSort<ContextOrder>(chains, config.sort);
  std::cerr << "Backoff" << std::endl;
  chains >> Backoff();

  BlockingSort<SuffixOrder>(chains, config.sort);
  std::cerr << "Print" << std::endl;
  VocabReconstitute vocab(vocab_file.get());
  chains >> Print<ProbBackoff>(vocab, out) >> util::stream::kRecycle;
  chains.Wait();
}

}} // namespaces
