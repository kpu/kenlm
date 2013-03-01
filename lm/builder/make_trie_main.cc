#include "lm/builder/binarize.hh"
#include "lm/builder/multi_stream.hh"
#include "lm/builder/renumber.hh"
#include "lm/builder/sort.hh"
#include "util/file.hh"
#include "util/stream/io.hh"

#include <boost/lexical_cast.hpp>

int main() {
  util::scoped_fd vocab(util::OpenReadOrThrow("vocab"));
  lm::builder::FixedArray<util::scoped_fd> files(5);
  std::vector<uint64_t> counts;
  for (unsigned i = 1; i <= 5; ++i) {
    files.push_back(util::OpenReadOrThrow(boost::lexical_cast<std::string>(i).c_str()));
    uint64_t size = util::SizeOrThrow(files.back().get());
    counts.push_back(size / (8 + 4 * i));
  }
  lm::ngram::Config config;
  config.pointer_bhiksha_bits = 64;
  config.write_mmap = "trie";
  std::vector<lm::WordIndex> mapping;
  lm::builder::Binarize binarize(counts, config, vocab.get(), mapping);

  util::stream::SortConfig sort_config;
  sort_config.temp_prefix = ".";
  sort_config.buffer_size = 64 << 20;
  sort_config.total_memory = 1 << 30;
  lm::builder::Sorts<lm::builder::SuffixOrder> sorts;
  sorts.Init(5);
  std::size_t lazy_memory = 1048576;
  for (unsigned i = 0; i < 5; ++i) {
    util::stream::ChainConfig config(lm::builder::NGram::TotalSize(i + 1), 2, sort_config.total_memory);
    util::stream::Chain chain(config);
    std::cerr << "Reading order " << (i+1) << std::endl;
    chain.ActivateProgress();
    chain.SetProgressTarget(counts[i] * (8 + 4 * i));
    chain >> util::stream::Read(files[i].get()) >> lm::builder::Renumber(&mapping[0]);
    sorts.push_back(chain, sort_config, lm::builder::SuffixOrder(i + 1));
    chain.Wait();
    sorts.back().Merge(lazy_memory);
  }

  std::size_t individual_memory = 1048576;
  lm::builder::Chains chains(5);
  std::cerr << "Converting to trie." << std::endl;
  for (unsigned i = 0; i < 5; ++i) {
    chains.push_back(util::stream::ChainConfig(lm::builder::NGram::TotalSize(i + 1), 2, individual_memory));
    if (i == 4) chains.back().ActivateProgress();
    sorts[i].Output(chains[i], lazy_memory);
  }
  chains >> boost::ref(binarize) >> util::stream::kRecycle;
  chains.Wait();
}
