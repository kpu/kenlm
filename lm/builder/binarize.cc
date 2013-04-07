#include "lm/builder/binarize.hh"

#include "lm/builder/joint_order.hh"
#include "lm/builder/ngram.hh"
#include "lm/builder/sort.hh"
#include "lm/quantize.hh"

#include <assert.h>

namespace lm { namespace builder {

Binarize::Binarize(uint64_t unigram_count, uint8_t order, const ngram::Config &config, int vocab_file, std::vector<WordIndex> &mapping)
  : model_(unigram_count, order, config), config_(config) {
  {
    util::scoped_fd permuted_text(util::CreateOrThrow("vocab_permuted"));
    ngram::SizedWriteWordsWrapper writer(NULL, permuted_text.get(), 0);
    model_.vocab_.ConfigureEnumerate(&writer, unigram_count);
    model_.vocab_.BuildFromFile(vocab_file, mapping);
  }
  end_sentence_ = model_.vocab_.EndSentence();

  // forget vocab until later
  vocab_size_ = model_.backing_.vocab.size();
  model_.backing_.vocab.reset();
}

void Binarize::SetupSearch(const std::vector<uint64_t> &counts) {
  counts_ = counts;
  uint64_t search_size = Model::SearchBackend::Size(counts_, config_);
  util::ResizeOrThrow(model_.backing_.file.get(), vocab_size_ + search_size);
  util::Rolling mem(model_.backing_.file.get(), true, 64 << 20, 1024, vocab_size_, search_size);
  model_.search_.SetupMemory(mem, counts_, config_);
}

void Binarize::Run(const ChainPositions &positions) {
  assert(counts_.size()); // SetupSearch has been called
  JointOrder<Binarize, SuffixOrder>(positions, *this);
}

void Binarize::Finish() {
  model_.search_.ExternalFinished(config_, counts_[0]);
  model_.backing_.search.reset();
  model_.backing_.vocab.reset(util::MapOrThrow(vocab_size_, true, util::kFileFlags, false, model_.backing_.file.get()), vocab_size_, util::scoped_memory::MMAP_ALLOCATED);
  model_.ExternalFinish(config_, counts_);
}

}} // namespaces
