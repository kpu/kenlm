#include "lm/interpolate/pipeline.hh"

#include "lm/common/renumber.hh"
#include "lm/interpolate/interpolate_info.hh"
#include "lm/interpolate/merge_vocab.hh"
#include "lm/interpolate/universal_vocab.hh"
#include "util/stream/chain.hh"
#include "util/stream/multi_stream.hh"
#include "util/fixed_array.hh"

namespace lm { namespace interpolate { namespace {

/* Put the original input files on chains and renumber them */
void SetupInputs(std::size_t buffer_size, const UniversalVocab &vocab, util::FixedArray<ModelBuffer> &models, util::FixedArray<util::stream::Chains> &chains) {
  chains.Init(models.size());
  // TODO: much better memory sizing heuristics e.g. not making the chain larger than it will use.
  util::stream::ChainConfig config(0, 2, buffer_size);
  for (std::size_t i = 0; i < models.size(); ++i) {
    chains.push_back(models[i].Order());
    for (std::size_t j = 0; j < models[i].Order(); ++j) {
      config.entry_size = sizeof(WordIndex) * (j + 1) + sizeof(float) * 2; // TODO do not include wasteful backoff for highest.
      chains.back().push_back(config);
    }
    models[i].Source(chains.back());
    for (std::size_t j = 0; j < models[i].Order(); ++j) {
      chains[i][j] >> Renumber(vocab.Mapping(i), j + 1);
    }
  }
}
} // namespace

void Pipeline(util::FixedArray<ModelBuffer> &models, const Config &config) {
  // Setup InterpolateInfo and UniversalVocab.
  InterpolateInfo info;
  info.lambdas = config.lambdas;
  std::vector<WordIndex> vocab_sizes;
  util::FixedArray<util::scoped_fd> vocab_files;
  for (ModelBuffer *i = models.begin(); i != models.end(); ++i) {
    info.orders.push_back(i->Order());
    vocab_sizes.push_back(i->Counts()[0]);
    vocab_files.push_back(util::DupOrThrow(i->VocabFile()));
  }
  UniversalVocab vocab(vocab_sizes);
  MergeVocabIndex(vocab_files, vocab);

  // Pass 1: merge probabilities
  util::FixedArray<util::stream::Chains> input_chains;
  SetupInputs(config.buffer_size, vocab, models, input_chains);
  util::FixedArray<util::stream::ChainPositions> models_by_order;
  for (std::size_t i = 0; i < input_chains.size(); ++i) {
    models_by_order.push_back(input_chains[i]);
  }
  // TODO: extract PartialProbGamma construction from MergeProbabilities to initialize its output chains and the inputs for pass 2.
}

}} // namespaces
