#include "lm/builder/binarize.hh"
#include "lm/builder/multi_stream.hh"
#include "lm/builder/renumber.hh"
#include "lm/builder/sort.hh"
#include "lm/builder/train_quantizer.hh"
#include "util/file.hh"
#include "util/stream/io.hh"

#include <boost/lexical_cast.hpp>
#include <boost/scoped_ptr.hpp>

int main() {
  const unsigned int order = 5;
  const std::size_t lazy_memory = 10ULL << 30;
  const std::size_t individual_memory = 1 << 30;

  util::scoped_fd vocab(util::OpenReadOrThrow("vocab"));
  lm::builder::FixedArray<util::scoped_fd> files(order);
  std::vector<uint64_t> counts;
  for (unsigned i = 1; i <= order; ++i) {
    files.push_back(util::OpenReadOrThrow(boost::lexical_cast<std::string>(i).c_str()));
//    uint64_t size = util::SizeOrThrow(files.back().get());
//    counts.push_back(size / (8 + 4 * i));
  }
  counts.push_back(393633486);
  counts.push_back(3732525120);
  counts.push_back(17521360103ULL);
  counts.push_back(39878926366ULL);
  counts.push_back(59847204733ULL);

  lm::ngram::Config config;
  config.pointer_bhiksha_bits = 64;
  config.write_mmap = "trie";
  config.prob_bits = config.backoff_bits = 10;
  std::vector<lm::WordIndex> mapping;
  lm::builder::Binarize binarize(counts, config, vocab.get(), mapping);

  util::stream::SortConfig sort_config;
  sort_config.temp_prefix = "/disk2/";
  sort_config.buffer_size = 64 << 20;
  sort_config.total_memory = 90ULL << 30;
  lm::builder::Sorts<lm::builder::SuffixOrder> sorts;
  sorts.Init(order);

  lm::builder::QuantizeTrainer::Config quant_config;
  quant_config.sort = sort_config;
  quant_config.adding_memory = 64 << 20;
  quant_config.block_count = 2;
  lm::builder::QuantizeProb quant_longest(quant_config);

  for (unsigned i = 0; i < order; ++i) {
    util::stream::ChainConfig config(lm::builder::NGram::TotalSize(i + 1), 2, sort_config.total_memory);
    util::stream::Chain chain(config);
    std::cerr << "Reading order " << (i+1) << std::endl;
    chain.ActivateProgress();
    chain.SetProgressTarget(counts[i] * (8 + 4 * (i + 1)));
    chain >> util::stream::Decompress(files[i].release()) >> lm::builder::Renumber(&mapping[0]);
    boost::scoped_ptr<lm::builder::QuantizeProbBackoff> quant_middle;
    if (i == order - 1) {
      chain >> boost::ref(quant_longest);
    } else if (i != 0) {
      quant_middle.reset(new lm::builder::QuantizeProbBackoff(quant_config));
      chain >> boost::ref(*quant_middle);
    }
    sorts.push_back(chain, sort_config, lm::builder::SuffixOrder(i + 1));
    chain.Wait();
    sorts.back().Merge(lazy_memory);
    std::cerr << "Training quantizer" << std::endl;
    if (i == order - 1) {
      quant_longest.Train(binarize.Quantizer().LongestTable());
    } else if (i != 0) {
      quant_middle->Train(binarize.Quantizer().MiddleTable(i - 1));
    }
  }
  binarize.Quantizer().FinishedLoading(config);

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
