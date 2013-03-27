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
  sort_config.temp_prefix = "/disk1/heafield/";
  sort_config.buffer_size = 64 << 20;
  sort_config.total_memory = 90ULL << 30;
  lm::builder::Sorts<lm::builder::SuffixOrder> sorts;
  sorts.Init(order);

/*  lm::builder::QuantizeTrainer::Config quant_config;
  quant_config.sort = sort_config;
  quant_config.adding_memory = 64 << 20;
  quant_config.block_count = 2;
  lm::builder::QuantizeProb quant_longest(quant_config);*/

  for (unsigned i = 0; i < order; ++i) {
    util::stream::Chain chain(util::stream::ChainConfig(lm::builder::NGram::TotalSize(i + 1), 2, sort_config.total_memory));
    std::cerr << "Reading order " << (i+1) << std::endl;
    chain.ActivateProgress();
    chain.SetProgressTarget(counts[i] * (8 + 4 * i));
    chain >> util::stream::Read(files[i].get()) >> lm::builder::Renumber(&mapping[0]);
    boost::scoped_ptr<lm::builder::QuantizeProbBackoff> quant_middle;
/*    if (i == order - 1) {
//      sort_config.temp_prefix = "/disk6/heafield/";
      chain >> boost::ref(quant_longest);
    } else if (i != 0) {
      quant_middle.reset(new lm::builder::QuantizeProbBackoff(quant_config));
      chain >> boost::ref(*quant_middle);
    }*/
    util::stream::SortConfig copied(sort_config);
    copied.temp_prefix += boost::lexical_cast<std::string>(i + 1);
    sorts.push_back(chain, copied, lm::builder::SuffixOrder(i + 1));
    chain.Wait();
    sorts.back().Merge(lazy_memory);
    std::cerr << "Training quantizer" << std::endl;
    if (i == order - 1) {
//      quant_longest.Train(binarize.Quantizer().LongestTable());
      util::ReadOrThrow(util::scoped_fd(util::OpenReadOrThrow("quant_prob_5")).get(), binarize.Quantizer().LongestTable().Populate(), sizeof(float) << config.prob_bits);
    } else if (i != 0) {
//      quant_middle->Train(binarize.Quantizer().MiddleTable(i - 1));
      std::string number = boost::lexical_cast<std::string>(i + 1);
      util::ReadOrThrow(util::scoped_fd(util::OpenReadOrThrow((std::string("quant_prob_") + number).c_str())).get(), binarize.Quantizer().MiddleTable(i-1)[0].Populate(), sizeof(float) << config.prob_bits); 
      util::ReadOrThrow(util::scoped_fd(util::OpenReadOrThrow((std::string("quant_backoff_") + number).c_str())).get(), binarize.Quantizer().MiddleTable(i-1)[1].Populate(), sizeof(float) << config.backoff_bits); 
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
