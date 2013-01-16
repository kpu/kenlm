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
      : config_(config), chains_(config.order), files_(config.order) {
      config_.minimum_block = std::max(NGram::TotalSize(config_.order), config_.minimum_block);
    }

    const PipelineConfig &Config() const { return config_; }

    // This takes the (partially) sorted ngrams and sets up for adjusted counts.
    void InitForAdjust(util::stream::Sort<SuffixOrder, AddCombiner> &ngrams, WordIndex types) {
      const std::size_t each_order_min = config_.minimum_block * config_.chain.block_count;
      // We know how many unigrams there are.  Don't allocate more than needed to them.
      const std::size_t min_chains = (config_.order - 1) * each_order_min +
        std::min(types * NGram::TotalSize(1), each_order_min);
      // Do merge sort, decide whether we're lazy, and calculate how much memory is left.
      const std::size_t leftovers = config_.TotalMemory() - 
        ngrams.Merge(std::min(config_.TotalMemory() - min_chains, ngrams.DefaultLazy()));

      std::vector<uint64_t> count_bounds(1, types);
      CreateChains(leftovers, count_bounds);
      ngrams.Output(chains_.back());

      // Setup unigram file.  
      files_.push_back(util::MakeTemp(config_.TempPrefix()));
    }

    Chains &MutableChains() { return chains_; }

    template <class T> Master &operator>>(const T &worker) {
      chains_ >> worker;
      return *this;
    }

    template <class Compare> void Sort(const char *contents, const char *banner) {
      Sorts<Compare> sorts;
      SetupSorts(sorts);
      std::cerr << banner << std::endl;
      // Call merge first to prevent it from using memory at the same time as the chain.  
      for (std::size_t i = 1; i < config_.order; ++i) {
        sorts[i - 1].Merge(sorts[i - 1].DefaultLazy());
      }
      chains_[0] >> files_[0].Source();
      for (std::size_t i = 1; i < config_.order; ++i) {
        sorts[i - 1].Output(chains_[i]);
      }
      // TODO: conflicting with progress bar.  
      //std::cerr << "[" << ToMB(bytes) << " MB] " << contents << std::endl;
    }

    void SortAndReadTwice(const std::vector<uint64_t> &counts, Sorts<ContextOrder> &sorts, Chains &second, util::stream::ChainConfig second_config) {
      // Do merge first before allocating chain memory.
      for (std::size_t i = 1; i < config_.order; ++i) {
        sorts[i - 1].Merge(0);
      }
      // There's no lazy merge, so just divide memory amongst the chains.
      CreateChains(config_.TotalMemory(), counts);
      chains_[0] >> files_[0].Source();
      second_config.entry_size = NGram::TotalSize(1);
      second.push_back(second_config);
      second.back() >> files_[0].Source();
      for (std::size_t i = 1; i < config_.order; ++i) {
        util::scoped_fd fd(sorts[i - 1].StealCompleted());
        if (i == config_.order - 1)
          chains_[i].ActivateProgress();
        chains_[i].SetProgressTarget(util::SizeOrThrow(fd.get()));
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
    }

  private:
    // Create chains, allocating memory to them.  Totally heuristic.  Count
    // bounds are upper bounds on the counts or not present.
    void CreateChains(std::size_t remaining_mem, const std::vector<uint64_t> &count_bounds) {
      std::vector<std::size_t> assignments;
      assignments.reserve(config_.order);
      // Start by assigning maximum memory usage (to be refined later).
      for (std::size_t i = 0; i < count_bounds.size(); ++i) {
        assignments.push_back(static_cast<std::size_t>(std::min(
            static_cast<uint64_t>(remaining_mem),
            count_bounds[i] * static_cast<uint64_t>(NGram::TotalSize(i + 1)))));
      }
      assignments.resize(config_.order, remaining_mem);

      // Now we know how much memory everybody wants.  How much will they get?
      // Proportional to this.
      std::vector<float> portions;
      // Indices of orders that have yet to be assigned.
      std::vector<std::size_t> unassigned;
      for (std::size_t i = 0; i < config_.order; ++i) {
        portions.push_back(static_cast<float>((i+1) * NGram::TotalSize(i+1)));
        unassigned.push_back(i);
      }
      /*If somebody doesn't eat their full dinner, give it to the rest of the
       * family.  Then somebody else might not eat their full dinner etc.  Ends
       * when everybody unassigned is hungry.
       */
      float sum;
      bool found_more;
      std::vector<std::size_t> block_count(config_.order);
      do {
        sum = 0.0;
        for (std::size_t i = 0; i < unassigned.size(); ++i) {
          sum += portions[unassigned[i]];
        }
        found_more = false;
        // If the proportional assignment is more than needed, give it just what it needs.
        for (std::vector<std::size_t>::iterator i = unassigned.begin(); i != unassigned.end();) {
          if (assignments[*i] <= remaining_mem * (portions[*i] / sum)) {
            remaining_mem -= assignments[*i];
            block_count[*i] = 1;
            i = unassigned.erase(i);
            found_more = true;
          } else {
            ++i;
          }
        }
      } while (found_more);
      for (std::vector<std::size_t>::iterator i = unassigned.begin(); i != unassigned.end(); ++i) {
        assignments[*i] = remaining_mem * (portions[*i] / sum);
        block_count[*i] = config_.chain.block_count;
      }
      chains_.clear();
      std::cerr << "Assigning chain sizes";
      for (std::size_t i = 0; i < config_.order; ++i) {
        std::cerr << ' ' << (i+1) << ":" << assignments[i];
        chains_.push_back(util::stream::ChainConfig(NGram::TotalSize(i + 1), block_count[i], assignments[i]));
      }
      std::cerr << std::endl;
    }

    PipelineConfig config_;

    Chains chains_;
    // Often only unigrams, but sometimes all orders.  
    FixedArray<util::stream::FileBuffer> files_;
};

void CountText(int text_file /* input */, int vocab_file /* output */, Master &master, uint64_t &token_count, std::string &text_file_name) {
  const PipelineConfig &config = master.Config();
  std::cerr << "=== 1/5 Counting and sorting n-grams ===" << std::endl;

  UTIL_THROW_IF(config.TotalMemory() < config.assume_vocab_hash_size, util::Exception, "Vocab hash size estimate " << config.assume_vocab_hash_size << " exceeds total memory " << config.TotalMemory());
  std::size_t memory_for_chain = 
    // This much memory to work with after vocab hash table.
    static_cast<float>(config.TotalMemory() - config.assume_vocab_hash_size) /
    // Solve for block size including the dedupe multiplier for one block.
    (static_cast<float>(config.chain.block_count) + CorpusCount::DedupeMultiplier(config.order)) *
    // Chain likes memory expressed in terms of total memory.
    static_cast<float>(config.chain.block_count);
  util::stream::Chain chain(util::stream::ChainConfig(NGram::TotalSize(config.order), config.chain.block_count, memory_for_chain));

  WordIndex type_count;
  util::FilePiece text(text_file, NULL, &std::cerr);
  text_file_name = text.FileName();
  chain >> CorpusCount(text, vocab_file, token_count, type_count);

  util::stream::Sort<SuffixOrder, AddCombiner> sorter(chain, config.sort, SuffixOrder(config.order), AddCombiner());
  chain.Wait(true);
  master.InitForAdjust(sorter, type_count);
}

void InitialProbabilities(const std::vector<uint64_t> &counts, const std::vector<Discount> &discounts, Master &master, FixedArray<util::stream::FileBuffer> &gammas) {
  const PipelineConfig &config = master.Config();
  Chains second(config.order);

  {
    Sorts<ContextOrder> sorts;
    master.SetupSorts(sorts);
    PrintStatistics(counts, discounts);
    lm::ngram::ShowSizes(counts);
    std::cerr << "=== 3/5 Calculating and sorting initial probabilities ===" << std::endl;
    master.SortAndReadTwice(counts, sorts, second, config.initial_probs.adder_in);
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
  util::stream::ChainConfig read_backoffs(config.read_backoffs);
  read_backoffs.entry_size = sizeof(float);
  for (std::size_t i = 0; i < config.order - 1; ++i) {
    gamma_chains.push_back(read_backoffs);
    gamma_chains.back() >> gammas[i].Source();
  }
  master >> Interpolate(unigram_count, ChainPositions(gamma_chains));
  gamma_chains >> util::stream::kRecycle;
  master.BufferFinal();
}

} // namespace

void Pipeline(const PipelineConfig &config, int text_file, std::ostream &out) {
  UTIL_TIMER("[%w s] Total wall time elapsed\n");
  Master master(config);

  util::scoped_fd vocab_file(config.vocab_file.empty() ? 
      util::MakeTemp(config.TempPrefix()) : 
      util::CreateOrThrow(config.vocab_file.c_str()));
  uint64_t token_count;
  std::string text_file_name;
  CountText(text_file, vocab_file.get(), master, token_count, text_file_name);

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
  HeaderInfo header_info(text_file_name, token_count, config.order, interpolate_orders);
  master >> PrintARPA(vocab, counts, (config.verbose_header ? &header_info : NULL), out) >> util::stream::kRecycle;
  master.MutableChains().Wait(true);
}

}} // namespaces
