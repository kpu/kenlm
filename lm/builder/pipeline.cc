#include "lm/builder/pipeline.hh"

#include "lm/builder/adjust_counts.hh"
#include "lm/builder/backoff.hh"
#include "lm/builder/corpus_count.hh"
#include "lm/builder/initial_probabilities.hh"
#include "lm/builder/interpolate.hh"
#include "lm/builder/print.hh"
#include "lm/builder/sort.hh"

#include "util/file.hh"
#include "util/stream/io.hh"

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
  boost::timer::auto_cpu_timer t(std::cerr, 1, "Total wall time elapsed: %w seconds\n");

  util::scoped_fd vocab_file(config.vocab_file.empty() ? 
      util::MakeTemp(config.TempPrefix()) : 
      util::CreateOrThrow(config.vocab_file.c_str()));
  std::vector<util::stream::ChainConfig> chain_configs(config.order, config.chain);
  for (std::size_t i = 0; i < config.order; ++i) {
    chain_configs[i].entry_size = NGram::TotalSize(i + 1);
  }

  // create "chains", one per order that largely work in parallel
  Chains chains(chain_configs);

  // initially, we only need counts for the highest order n-grams
  // so we'll operate just on chains[config.order-1]
  // TODO: Don't stick this in the middle of a progress bar
  std::cerr << "Counting and sorting n-grams" << std::endl;
  CorpusCount corpus_count(text, config.order, vocab_file.get());
  chains[config.order - 1] >> boost::ref(corpus_count);
  BlockingSort(chains[config.order - 1], config.sort, SuffixOrder(config.order), AddCombiner(), "Preparing to adjust counts");

  std::cerr << "Calculating and sorting adjusted counts" << std::endl;
  std::vector<uint64_t> counts;
  std::vector<Discount> discounts;
  chains >> AdjustCounts(counts, discounts);
  util::stream::FileBuffer unigrams(util::MakeTemp(config.TempPrefix()));

  // Uninterpolated probabilities reads the file twice in separate threads, hence the special handling.  
  { // scope the sorts object

    // we don't just call BlockingSort here because it would also put a MergingReader on the chain
    // instead, we'll have InitialProbabilities use a custom method to fully sort everything and then
    // extract two separate readers
    Sorts<ContextOrder> sorts(unigrams, chains, config.sort);

    { // scope the timer
      boost::timer::auto_cpu_timer t(std::cerr, 1, "Preparing to calculate initial probabilities: Finishing partial merge sort took %w seconds\n");
      chains.Wait(true);
    }
    PrintStatistics(counts, discounts);

    std::cerr << "Calculating and sorting initial probabilities" << std::endl;
    InitialProbabilities(config.initial_probs, discounts, sorts, chains);
  }
  BlockingSort<SuffixOrder>(unigrams, chains, config.sort, "Preparing to interpolate orders");
  std::cerr << "Calculating and sorting order-interpolated probabilities" << std::endl;
  chains >> Interpolate(counts[0]);

  BlockingSort<PrefixOrder>(unigrams, chains, config.sort, "Preparing to renormalize backoff weights");
  std::cerr << "Calculating backoff weights" << std::endl;
  chains >> Backoff();

  // we don't *need* to sort after renormalizing the backoff weights to still produce a valid ARPA file
  std::vector<boost::shared_ptr<util::stream::FileBuffer> > file_buffers; // must survive until PrintARPA is done
  if (config.sorted_arpa) {
    uint64_t backoff_bytes = BlockingSort<SuffixOrder>(unigrams, chains, config.sort, "Preparing to write ARPA file");
    std::cerr << "[" << ToMB(backoff_bytes) << " MB] Renormalized backoff weights" << std::endl;
  } else { 
    // allocate a temporary holding location on disk (we'll use the same temp space as sorting)
    for (size_t i = 0; i < config.order; ++i) {
      file_buffers.push_back(boost::shared_ptr<util::stream::FileBuffer>(
        new util::stream::FileBuffer(util::MakeTemp(config.sort.temp_prefix))));
      chains[i] >> file_buffers.at(i)->Sink();
    }
    // wait for backoff weights to finish writing
    chains.Wait();
    // begin reading from disk again
    for (size_t i = 0; i < config.order; ++i) {
      chains[i] >> file_buffers.at(i)->Source();
    }
  }

  std::cerr << "=== 6/6 Writing ARPA model ===" << std::endl;
  VocabReconstitute vocab(vocab_file.get());
  bool interpolate_orders = true;
  HeaderInfo header_info(text.FileName(), corpus_count.TokenCount(), config.order, interpolate_orders);
  chains >> PrintARPA(vocab, counts, (config.verbose_header ? &header_info : NULL), out) >> util::stream::kRecycle;
  chains.Wait();
}

}} // namespaces
