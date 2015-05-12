#include "lm/interpolate/backoff_reunification.hh"
#include "lm/builder/model_buffer.hh"
#include "lm/builder/ngram.hh"
#include "lm/builder/sort.hh"

#include <cassert>

namespace lm {
namespace interpolate {

class MergeWorker {
public:
  MergeWorker(std::size_t order,
              boost::reference_wrapper<util::stream::Stream> prob_input,
              boost::reference_wrapper<util::stream::Stream> boff_input)
      : order_(order), prob_input_(prob_input), boff_input_(boff_input) {
    // nothing
  }

  void Run(const util::stream::ChainPosition &position) {
    lm::builder::NGramStream stream(position);

    for (; prob_input_.get() && boff_input_.get();
         ++prob_input_.get(), ++boff_input_.get(), ++stream) {

      const WordIndex *start
          = reinterpret_cast<const WordIndex *>(prob_input_.get().Get());
      const WordIndex *end = start + order_;
      std::copy(start, end, stream->begin());

      stream->Value().complete.prob = *reinterpret_cast<const float *>(end);
      stream->Value().complete.backoff
          = *reinterpret_cast<float *>(boff_input_.get().Get());
    }
    stream.Poison();
  }

private:
  std::size_t order_;
  boost::reference_wrapper<util::stream::Stream> prob_input_;
  boost::reference_wrapper<util::stream::Stream> boff_input_;
};

void ReunifyBackoff(const ReunifyConfig &config,
                    util::stream::Chains &prob_chains,
                    util::stream::Chains &backoff_chains) {
  assert(prob_chains.size() == backoff_chains.size());

  util::stream::Chains output_chains(prob_chains.size());
  lm::builder::Sorts<lm::builder::SuffixOrder> sorts(prob_chains.size());
  for (size_t i = 0; i < prob_chains.size(); ++i) {
    output_chains.push_back(
        util::stream::ChainConfig(lm::builder::NGram::TotalSize(i + 1),
                                  config.num_blocks, config.max_ram));

    sorts.push_back(prob_chains[i], config.sort_config,
                    lm::builder::SuffixOrder(i + 1));
  }

  // Step 1: Suffix-sort the probability values
  for (size_t i = 0; i < prob_chains.size(); ++i) {
    prob_chains[i].Wait();
    sorts[i].Output(prob_chains[i]);
  }

  util::stream::Streams prob_input_streams;
  prob_chains >> prob_input_streams;

  util::stream::Streams boff_input_streams;
  backoff_chains >> boff_input_streams;

  // Step 2: Merge the backoff with the probability values
  for (size_t i = 0; i < prob_chains.size(); ++i) {
    output_chains[i] >> MergeWorker(i + 1, boost::ref(prob_input_streams[i]),
                                    boost::ref(boff_input_streams[i]));
  }

  // Step 3: Output to intermediate output format
  lm::builder::ModelBuffer buffer(config.file_base, true, false);
  buffer.Sink(output_chains);
}
}
}
