#include "lm/builder/pipeline.hh"

#include "lm/builder/adjust_counts.hh"
#include "lm/builder/corpus_count.hh"
#include "lm/builder/initial_probabilities.hh"
#include "lm/builder/interpolate.hh"
#include "lm/builder/print.hh"
#include "lm/builder/sort.hh"

#include "lm/sizes.hh"

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

uint64_t ToMB(uint64_t bytes) {
  return bytes / 1024 / 1024;
}

class Master {
  public:
    explicit Master(const PipelineConfig &config) 
      : config_(config), chains_(config.order), files_(config.order), in_memory_(config.order, false) {
      util::stream::ChainConfig chain_config = config.chain;
      for (std::size_t i = 0; i < config.order; ++i) {
        chain_config.entry_size =  NGram::TotalSize(i + 1);
        chains_.push_back(chain_config);
      }
      // Setup unigram file.  
      files_.push_back(util::MakeTemp(config_.TempPrefix()));
    }

    const PipelineConfig &Config() const { return config_; }

    Chains &MutableChains() { return chains_; }

    template <class T> Master &operator>>(const T &worker) {
      chains_ >> worker;
      return *this;
    }

    template <class Compare> void Sort(const char *contents, const char *banner) {
      Sorts<Compare> sorts;
      SetupSorts(sorts);
      std::cerr << banner << std::endl;
      uint64_t bytes = 0;
      for (std::size_t i = 1; i < config_.order; ++i) {
        bytes += sorts[i - 1].Output(chains_[i], i == config_.order - 1 ? &std::cerr : NULL);
      }
      // TODO: conflicting with progress bar.  
      //std::cerr << "[" << ToMB(bytes) << " MB] " << contents << std::endl;
    }

    void SortAndReadTwice(Sorts<ContextOrder> &sorts, Chains &second, util::stream::ChainConfig second_config) {
      second_config.entry_size = NGram::TotalSize(1);
      second.push_back(second_config);
      second.back() >> files_[0].Source();
      for (std::size_t i = 1; i < config_.order; ++i) {
        util::scoped_fd fd(sorts[i - 1].StealCompleted());
        if (i == config_.order - 1)
          chains_[i].ResetProgress(&std::cerr, util::SizeOrThrow(fd.get()));

        chains_[i] >> util::stream::PRead(util::DupOrThrow(fd.get()), true);
        second_config.entry_size = NGram::TotalSize(i + 1);
        second.push_back(second_config);
        second.back() >> util::stream::PRead(fd.release(), true);
      }
    }

    void BufferFinal() {
      chains_[0] >> files_[0].Sink();
      for (std::size_t i = 1; i < config_.order; ++i) {
        files_.push_back(util::MakeTemp(config_.TempPrefix()));
        chains_[i] >> files_[i].Sink();
      }
      chains_.Wait(true);
      for (std::size_t i = 0; i < config_.order; ++i) {
        chains_[i] >> files_[i].Source();
      }
    }

    template <class Compare> void SetupSorts(Sorts<Compare> &sorts) {
      sorts.Init(config_.order - 1);
      // Unigrams don't get sorted because their order is always the same.
      chains_[0] >> files_[0].Sink();
      for (std::size_t i = 1; i < config_.order; ++i) {
        sorts.push_back(chains_[i], config_.sort, Compare(i + 1));
      }
      chains_.Wait(true);
      chains_[0] >> files_[0].Source();
    }

  private:
    PipelineConfig config_;

    Chains chains_;
    // Often only unigrams, but sometimes all orders.  
    FixedArray<util::stream::FileBuffer> files_;

    std::vector<bool> in_memory_;
};

void InitialProbabilities(const std::vector<uint64_t> &counts, const std::vector<Discount> &discounts, Master &master, FixedArray<util::stream::FileBuffer> &gammas) {
  const PipelineConfig &config = master.Config();
  Chains second(config.order);

  {
    Sorts<ContextOrder> sorts;
    master.SetupSorts(sorts);
    PrintStatistics(counts, discounts);
    lm::ngram::ShowSizes(counts);
    std::cerr << "=== 3/5 Calculating and sorting initial probabilities ===" << std::endl;
    master.SortAndReadTwice(sorts, second, config.initial_probs.adder_in);
  }

  Chains gamma_chains(config.order);
  InitialProbabilities(config.initial_probs, discounts, master.MutableChains(), second, gamma_chains);
  // Don't care about gamma for 0.  
  gamma_chains[0] >> util::stream::kRecycle;
  gammas.Init(config.order - 1);
  for (std::size_t i = 1; i < config.order; ++i) {
    gammas.push_back(util::MakeTemp(config.TempPrefix()));
    gamma_chains[i] >> gammas[i - 1].Sink();
  }
  master.Sort<SuffixOrder>("Initial Probabilities", "=== 4/5 Calculating and sorting order-interpolated probabilities ===");
}

void InterpolateProbabilities(uint64_t unigram_count, Master &master, FixedArray<util::stream::FileBuffer> &gammas) {
  const PipelineConfig &config = master.Config();
  Chains gamma_chains(config.order - 1);
  for (std::size_t i = 0; i < config.order - 1; ++i) {
    gamma_chains.push_back(config.read_backoffs);
    gamma_chains.back() >> gammas[i].Source();
  }
  master >> Interpolate(unigram_count, ChainPositions(gamma_chains));
  gamma_chains >> util::stream::kRecycle;
  master.BufferFinal();
}

} // namespace

void Pipeline(PipelineConfig config, util::FilePiece &text, std::ostream &out) {
  UTIL_TIMER("[%w s] Total wall time elapsed\n");

  config.read_backoffs.entry_size = sizeof(float);

  util::scoped_fd vocab_file(config.vocab_file.empty() ? 
      util::MakeTemp(config.TempPrefix()) : 
      util::CreateOrThrow(config.vocab_file.c_str()));
  Master master(config);

  // initially, we only need counts for the highest order n-grams
  // so we'll operate just on chains[config.order-1]
  // TODO: Don't stick this in the middle of a progress bar
  std::cerr << "=== 1/5 Counting and sorting n-grams ===" << std::endl;
  uint64_t token_count;
  master.MutableChains()[config.order - 1] >> CorpusCount(text, vocab_file.get(), token_count);
  uint64_t corpus_count_bytes = BlockingSort(master.MutableChains()[config.order - 1], config.sort, SuffixOrder(config.order), AddCombiner(), "Preparing to adjust counts");
  std::cerr << "[" << ToMB(corpus_count_bytes) << " MB] N-gram counts" << std::endl;

  std::cerr << "=== 2/5 Calculating and sorting adjusted counts ===" << std::endl;
  std::vector<uint64_t> counts;
  std::vector<Discount> discounts;
  master >> AdjustCounts(counts, discounts);

  {
    FixedArray<util::stream::FileBuffer> gammas;
    InitialProbabilities(counts, discounts, master, gammas);
    InterpolateProbabilities(counts[0], master, gammas);
  }

  std::cerr << "=== 5/5 Writing ARPA model ===" << std::endl;
  VocabReconstitute vocab(vocab_file.get());
  bool interpolate_orders = true;
  HeaderInfo header_info(text.FileName(), token_count, config.order, interpolate_orders);
  master >> PrintARPA(vocab, counts, (config.verbose_header ? &header_info : NULL), out) >> util::stream::kRecycle;
  master.MutableChains().Wait(true);
}

}} // namespaces
