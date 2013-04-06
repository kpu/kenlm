#include "lm/builder/binarize.hh"

#include "lm/builder/joint_order.hh"
#include "lm/builder/ngram.hh"
#include "lm/builder/sort.hh"
#include "lm/quantize.hh"

namespace lm { namespace builder {

namespace {
typedef ngram::trie::TrieSearch<ngram::SeparatelyQuantize, ngram::trie::ArrayBhiksha> Search;
} // namespace

Binarize::Binarize(const std::vector<uint64_t> &counts, const ngram::Config &config, int vocab_file, std::vector<WordIndex> &mapping)
  : model_(counts, config), counts_(counts), config_(config) {
  {
    util::scoped_fd permuted_text(util::CreateOrThrow("vocab_permuted"));
    ngram::SizedWriteWordsWrapper writer(NULL, permuted_text.get(), 0);
    model_.vocab_.ConfigureEnumerate(&writer, counts[0]);
    model_.vocab_.BuildFromFile(vocab_file, mapping);
  }
  end_sentence_ = model_.vocab_.EndSentence();
  uint64_t search_size = Search::Size(counts_, config_);
  util::ResizeOrThrow(model_.backing_.file.get(), model_.backing_.vocab.size() + search_size);
  util::Rolling mem(model_.backing_.file.get(), true, 64 << 20, 1024, model_.backing_.vocab.size(), search_size);
  model_.search_.SetupMemory(mem, counts_, config_);

  // forget vocab until later
  vocab_size_ = model_.backing_.vocab.size();
  model_.backing_.vocab.reset();
}

namespace {
class Callback {
  public:
    Callback(Search &search, const WordIndex end_sentence)
      : search_(search), end_sentence_(end_sentence) {}

    void Enter(unsigned int, NGram &gram) {
      search_.ExternalInsert(gram.Order(), *gram.begin(), gram.Value().complete);
    }

    void Exit(unsigned int order_minus_1, const NGram &gram) {}

  private:
    Search &search_;
    const WordIndex end_sentence_;
};
} // namespace

void Binarize::Run(const ChainPositions &positions) {
  Callback callback(model_.search_, end_sentence_);
  JointOrder<Callback, SuffixOrder>(positions, callback);
  model_.search_.ExternalFinished(config_, counts_[0]);
  model_.backing_.search.reset();
  model_.backing_.vocab.reset(util::MapOrThrow(vocab_size_, true, util::kFileFlags, false, model_.backing_.file.get()), vocab_size_, util::scoped_memory::MMAP_ALLOCATED);
  model_.ExternalFinish(config_, counts_);
}

}} // namespaces
