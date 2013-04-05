#include "lm/builder/binarize.hh"
#include "lm/builder/multi_stream.hh"
#include "lm/builder/renumber.hh"
#include "lm/builder/sort.hh"
#include "lm/builder/train_quantizer.hh"
#include "util/file.hh"
#include "util/stream/io.hh"

#include <boost/lexical_cast.hpp>
#include <boost/scoped_ptr.hpp>

std::size_t LazyMemory(unsigned int order_minus_1) {
  if (order_minus_1 == 0) return 4717440456ULL;
  if (order_minus_1 == 4) return 30ULL << 30;
  return 20ULL << 30;
}

int main() {
  const unsigned int order = 5;

  util::scoped_fd vocab(util::OpenReadOrThrow("vocab"));
  lm::builder::FixedArray<util::scoped_fd> files(order);
  std::vector<uint64_t> counts;
  for (unsigned i = 1; i <= order; ++i) {
    files.push_back(util::OpenReadOrThrow(boost::lexical_cast<std::string>(i).c_str()));
    uint64_t size = util::SizeOrThrow(files.back().get());
    counts.push_back(size / (8 + 4 * i));
  }
  lm::ngram::Config config;
  config.pointer_bhiksha_bits = 64;
  config.write_mmap = "trie";
  config.prob_bits = config.backoff_bits = 10;
  std::vector<lm::WordIndex> mapping;
  lm::builder::Binarize binarize(counts, config, vocab.get(), mapping);

  util::stream::SortConfig sort_config;
  sort_config.temp_prefix = "/disk2/heafield/";
  sort_config.buffer_size = 64 << 20;
  sort_config.total_memory = 100ULL << 30;
  lm::builder::Sorts<lm::builder::SuffixOrder> sorts;
  sorts.Init(order);

  lm::builder::Chains chains(5);
  for (unsigned i = 0; i < order; ++i) {
    std::size_t chain_mem;
    if (i == 0) {
      chain_mem = 2ULL << 30;
    } else if (i == 1) {
      chain_mem = 18ULL << 30;
    } else if (i == order - 1) {
      chain_mem = 50ULL << 30;
    } else {
      chain_mem = 20ULL << 30;
    }
    chains.push_back(util::stream::ChainConfig(lm::builder::NGram::TotalSize(i + 1), 2, chain_mem));
    if (i == order - 1) {
      sort_config.temp_prefix = "/disk6/heafield/";
      chains.back().ActivateProgress();
      chains.back().SetProgressTarget(counts[i] * (8 + 4 * (i + 1)));
    }
    chains.back() >> util::stream::Read(files[i].get()) >> lm::builder::Renumber(&mapping[0]);
    sorts.push_back(chains.back(), sort_config, lm::builder::SuffixOrder(i + 1));
    if (i == order - 1) {
      util::ReadOrThrow(util::scoped_fd(util::OpenReadOrThrow("quant_prob_5")).get(), binarize.Quantizer().LongestTable().Populate(), sizeof(float) << config.prob_bits);
    } else if (i != 0) {
      std::string number = boost::lexical_cast<std::string>(i + 1);
      util::ReadOrThrow(util::scoped_fd(util::OpenReadOrThrow((std::string("quant_prob_") + number).c_str())).get(), binarize.Quantizer().MiddleTable(i-1)[0].Populate(), sizeof(float) << config.prob_bits); 
      util::ReadOrThrow(util::scoped_fd(util::OpenReadOrThrow((std::string("quant_backoff_") + number).c_str())).get(), binarize.Quantizer().MiddleTable(i-1)[1].Populate(), sizeof(float) << config.backoff_bits); 
    }
  }
  binarize.Quantizer().FinishedLoading(config);
  chains.Wait();
  for (unsigned i = 0; i < order; ++i) {
    sorts[i].Merge(LazyMemory(i));
  }

  std::cerr << "Converting to trie." << std::endl;
  chains.clear();
  for (unsigned i = 0; i < 5; ++i) {
    chains.push_back(util::stream::ChainConfig(lm::builder::NGram::TotalSize(i + 1), 2, 512ULL << 20));
    if (i == 4) chains.back().ActivateProgress();
    sorts[i].Output(chains[i], LazyMemory(i));
  }
  chains >> boost::ref(binarize) >> util::stream::kRecycle;
  chains.Wait();
}
