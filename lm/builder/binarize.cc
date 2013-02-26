#include "lm/builder/binarize.hh"

#include "lm/builder/joint_order.hh"
#include "lm/builder/ngram.hh"
#include "lm/builder/sort.hh"

namespace lm { namespace builder {

namespace {
typedef ngram::trie::TrieSearch<ngram::DontQuantize, ngram::trie::ArrayBhiksha> Search;
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
  // TODO: forget vocab?
}

namespace {
class Callback {
  public:
    Callback(Search &search, const WordIndex end_sentence)
      : search_(search), end_sentence_(end_sentence) {}

    void Enter(unsigned int, NGram &gram) {
      // TODO: remove this when no intermediate file does this anymore, because it's interpolation's responsibility.
      if (gram.Value().complete.backoff == 0.0) {
        WordIndex final_word = *(gram.end() - 1);
        // backoff is ignored for the longest order anyway.
        gram.Value().complete.backoff = (final_word == end_sentence_ || final_word == kUNK) ? ngram::kNoExtensionBackoff : ngram::kExtensionBackoff;
      }
      search_.ExternalInsert(gram.Order(), *gram.begin(), gram.Value().complete);
    }

    void Exit(unsigned int order_minus_1, const NGram &gram) {}

  private:
    Search &search_;
    const WordIndex end_sentence_;
};
} // namespace

void Binarize::Run(const ChainPositions &positions) {
  //// No need for unk padding space.
  //uint64_t write_words = model_.backing_.vocab.size() + Search::Size(counts, config);
  model_.search_.SetupMemory(ngram::GrowForSearch(config_, 0, Search::Size(counts_, config_), model_.backing_), counts_, config_);
  Callback callback(model_.search_, end_sentence_);
  JointOrder<Callback, SuffixOrder>(positions, callback);
  model_.search_.ExternalFinished(config_, counts_[0]);
  model_.ExternalFinish(config_, counts_);
}

}} // namespaces
