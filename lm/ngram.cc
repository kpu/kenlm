#include "lm/ngram.hh"

#include "lm/arpa_io.hh"
#include "lm/exception.hh"
#include "util/file_piece.hh"
#include "util/joint_sort.hh"
#include "util/probing_hash_table.hh"
#include "util/scoped.hh"

#include <algorithm>
#include <functional>
#include <numeric>
#include <string>

#include <cmath>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

namespace lm {
namespace ngram {
namespace detail {

void Prob::SetBackoff(float to) {
  UTIL_THROW(FormatLoadException, "Attempt to set backoff " << to << " for the highest order n-gram");
}

// Normally static initialization is a bad idea but MurmurHash is pure arithmetic, so this is ok.  
const uint64_t kUnknownHash = HashForVocab("<unk>", 5);
// Sadly some LMs have <UNK>.  
const uint64_t kUnknownCapHash = HashForVocab("<UNK>", 5);

} // namespace detail

SortedVocabulary::SortedVocabulary() : begin_(NULL), end_(NULL) {}

void SortedVocabulary::Init(void *start, std::size_t allocated, std::size_t entries) {
  assert(allocated >= Size(entries));
  begin_ = reinterpret_cast<Entry*>(start);
  end_ = begin_;
  saw_unk_ = false;
}

WordIndex SortedVocabulary::Insert(const StringPiece &str) {
  uint64_t hashed = detail::HashForVocab(str);
  if (hashed == detail::kUnknownHash || hashed == detail::kUnknownCapHash) {
    saw_unk_ = true;
    return 0;
  }
  end_->key = hashed;
  ++end_;
  // This is 1 + the offset where it was inserted to make room for unk.  
  return end_ - begin_;
}

void SortedVocabulary::FinishedLoading(detail::ProbBackoff *reorder_vocab) {
  util::JointSort(begin_, end_, reorder_vocab + 1);
  if (!saw_unk_) throw SpecialWordMissingException("<unk>");
  SetSpecial(Index("<s>"), Index("</s>"), 0, end_ - begin_ + 1);
}

namespace detail {

template <class Search> MapVocabulary<Search>::MapVocabulary() {}

template <class Search> void MapVocabulary<Search>::Init(void *start, std::size_t allocated, std::size_t entries) {
  lookup_ = Lookup(start, allocated);
  available_ = 1;
  // Later if available_ != expected_available_ then we can throw UnknownMissingException.
  saw_unk_ = false;
}

template <class Search> WordIndex MapVocabulary<Search>::Insert(const StringPiece &str) {
  uint64_t hashed = HashForVocab(str);
  // Prevent unknown from going into the table.  
  if (hashed == kUnknownHash || hashed == kUnknownCapHash) {
    saw_unk_ = true;
    return 0;
  } else {
    lookup_.Insert(Lookup::Packing::Make(hashed, available_));
    return available_++;
  }
}

template <class Search> void MapVocabulary<Search>::FinishedLoading(ProbBackoff *reorder_vocab) {
  lookup_.FinishedInserting();
  if (!saw_unk_) throw SpecialWordMissingException("<unk>");
  SetSpecial(Index("<s>"), Index("</s>"), 0, available_);
}

/* All of the entropy is in low order bits and boost::hash does poorly with
 * these.  Odd numbers near 2^64 chosen by mashing on the keyboard.  There is a
 * stable point: 0.  But 0 is <unk> which won't be queried here anyway.  
 */
inline uint64_t CombineWordHash(uint64_t current, const WordIndex next) {
  uint64_t ret = (current * 8978948897894561157ULL) ^ (static_cast<uint64_t>(next) * 17894857484156487943ULL);
  return ret;
}

uint64_t ChainedWordHash(const WordIndex *word, const WordIndex *word_end) {
  if (word == word_end) return 0;
  uint64_t current = static_cast<uint64_t>(*word);
  for (++word; word != word_end; ++word) {
    current = CombineWordHash(current, *word);
  }
  return current;
}

// Special unigram reader because unigram's data structure is different and because we're inserting vocab words.
template <class Voc> void Read1Grams(util::FilePiece &f, const size_t count, Voc &vocab, ProbBackoff *unigrams) {
  ReadNGramHeader(f, 1);
  for (size_t i = 0; i < count; ++i) {
    try {
      float prob = f.ReadFloat();
      if (f.get() != '\t') UTIL_THROW(FormatLoadException, "Expected tab after probability");
      ProbBackoff &value = unigrams[vocab.Insert(f.ReadDelimited())];
      value.prob = prob;
      switch (f.get()) {
        case '\t':
          value.SetBackoff(f.ReadFloat());
          if ((f.get() != '\n')) UTIL_THROW(FormatLoadException, "Expected newline after backoff");
          break;
        case '\n':
          value.ZeroBackoff();
          break;
        default:
          UTIL_THROW(FormatLoadException, "Expected tab or newline after unigram");
      }
     } catch(util::Exception &e) {
      e << " in the " << i << "th 1-gram at byte " << f.Offset();
      throw;
    }
  }
  if (f.ReadLine().size()) UTIL_THROW(FormatLoadException, "Expected blank line after unigrams at byte " << f.Offset());
}

template <class Voc, class Store> void ReadNGrams(util::FilePiece &f, const unsigned int n, const size_t count, const Voc &vocab, Store &store) {
  ReadNGramHeader(f, n);

  // vocab ids of words in reverse order
  WordIndex vocab_ids[n];
  typename Store::Packing::Value value;
  for (size_t i = 0; i < count; ++i) {
    try {
      value.prob = f.ReadFloat();
      for (WordIndex *vocab_out = &vocab_ids[n-1]; vocab_out >= vocab_ids; --vocab_out) {
        *vocab_out = vocab.Index(f.ReadDelimited());
      }
      uint64_t key = ChainedWordHash(vocab_ids, vocab_ids + n);

      switch (f.get()) {
        case '\t':
          value.SetBackoff(f.ReadFloat());
          if ((f.get() != '\n')) UTIL_THROW(FormatLoadException, "Expected newline after backoff");
          break;
        case '\n':
          value.ZeroBackoff();
          break;
        default:
          UTIL_THROW(FormatLoadException, "Expected tab or newline after unigram");
      }
      store.Insert(Store::Packing::Make(key, value));
    } catch(util::Exception &e) {
      e << " in the " << i << "th " << n << "-gram at byte " << f.Offset();
      throw;
    }
  }

  if (f.ReadLine().size()) UTIL_THROW(FormatLoadException, "Expected blank line after " << n << "-grams at byte " << f.Offset());
  store.FinishedInserting();
}

template <class Search, class VocabularyT> size_t GenericModel<Search, VocabularyT>::Size(const std::vector<size_t> &counts, const Config &config) {
  if (counts.size() < 2) UTIL_THROW(FormatLoadException, "This ngram implementation assumes at least a bigram model.");
  size_t memory_size = VocabularyT::Size(counts[0], config.probing_multiplier);
  memory_size += sizeof(ProbBackoff) * counts[0];
  for (unsigned char n = 2; n < counts.size(); ++n) {
    memory_size += Middle::Size(counts[n - 1], config.probing_multiplier);
  }
  memory_size += Longest::Size(counts.back(), config.probing_multiplier);
  return memory_size;
}

template <class Search, class VocabularyT> GenericModel<Search, VocabularyT>::GenericModel(const char *file, const Config &config) {
  util::FilePiece f(file, &std::cerr);

  std::vector<size_t> counts;
  ReadCounts(f, counts);

  if (counts.size() > kMaxOrder) UTIL_THROW(FormatLoadException, "This model has order " << counts.size() << ".  Edit ngram.hh's kMaxOrder to at least this value and recompile.");
  const size_t memory_size = Size(counts, config);
  unsigned char order = counts.size();

  memory_.reset(mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0), memory_size);
  if (memory_.get() == MAP_FAILED) throw AllocateMemoryLoadException(memory_size);

  size_t allocated;
  char *start = static_cast<char*>(memory_.get());
  allocated = VocabularyT::Size(counts[0], config.probing_multiplier);
  vocab_.Init(start, allocated, counts[0]);
  start += allocated;
  unigram_ = reinterpret_cast<ProbBackoff*>(start);
  start += sizeof(ProbBackoff) * counts[0];
  for (unsigned int n = 2; n < order; ++n) {
    allocated = Middle::Size(counts[n - 1], config.probing_multiplier);
    middle_.push_back(Middle(start, allocated));
    start += allocated;
  }
  allocated = Longest::Size(counts[order - 1], config.probing_multiplier);
  longest_ = Longest(start, allocated);
  assert(static_cast<size_t>(start + allocated - reinterpret_cast<char*>(memory_.get())) == memory_size);

  try {
    LoadFromARPA(f, counts);
  } catch (FormatLoadException &e) {
    e << " in file " << file;
    throw;
  }

  // g++ prints warnings unless these are fully initialized.  
  State begin_sentence = State();
  begin_sentence.valid_length_ = 1;
  begin_sentence.history_[0] = vocab_.BeginSentence();
  begin_sentence.backoff_[0] = unigram_[begin_sentence.history_[0]].backoff;
  State null_context = State();
  null_context.valid_length_ = 0;
  P::Init(begin_sentence, null_context, vocab_, order);
}

template <class Search, class VocabularyT> void GenericModel<Search, VocabularyT>::LoadFromARPA(util::FilePiece &f, const std::vector<size_t> &counts) {
  // Read the unigrams.
  Read1Grams(f, counts[0], vocab_, unigram_);
  vocab_.FinishedLoading(unigram_);
  
  // Read the n-grams.
  for (unsigned int n = 2; n < counts.size(); ++n) {
    ReadNGrams(f, n, counts[n-1], vocab_, middle_[n-2]);
  }
  ReadNGrams(f, counts.size(), counts[counts.size() - 1], vocab_, longest_);
  if (std::fabs(unigram_[0].backoff) > 0.0000001) UTIL_THROW(FormatLoadException, "Backoff for unknown word should be zero, but was given as " << unigram_[0].backoff);
}

/* Ugly optimized function.
 * in_state contains the previous ngram's length and backoff probabilites to
 * be used here.  out_state is populated with the found ngram length and
 * backoffs that the next call will find useful.  
 *
 * The search goes in increasing order of ngram length.  
 */
template <class Search, class VocabularyT> FullScoreReturn GenericModel<Search, VocabularyT>::FullScore(
    const State &in_state,
    const WordIndex new_word,
    State &out_state) const {

  FullScoreReturn ret;
  // This is end pointer passed to SumBackoffs.
  const ProbBackoff &unigram = unigram_[new_word];
  if (new_word == 0) {
    ret.ngram_length = out_state.valid_length_ = 0;
    // all of backoff.
    ret.prob = std::accumulate(
        in_state.backoff_,
        in_state.backoff_ + in_state.valid_length_,
        unigram.prob);
    return ret;
  }
  float *backoff_out(out_state.backoff_);
  *backoff_out = unigram.backoff;
  ret.prob = unigram.prob;
  out_state.history_[0] = new_word;
  if (in_state.valid_length_ == 0) {
    ret.ngram_length = out_state.valid_length_ = 1;
    // No backoff because NGramLength() == 0 and unknown can't have backoff.
    return ret;
  }
  ++backoff_out;

  // Ok now we now that the bigram contains known words.  Start by looking it up.

  uint64_t lookup_hash = static_cast<uint64_t>(new_word);
  const WordIndex *hist_iter = in_state.history_;
  const WordIndex *const hist_end = hist_iter + in_state.valid_length_;
  typename std::vector<Middle>::const_iterator mid_iter = middle_.begin();
  for (; ; ++mid_iter, ++hist_iter, ++backoff_out) {
    if (hist_iter == hist_end) {
      // Used history [in_state.history_, hist_end) and ran out.  No backoff.  
      std::copy(in_state.history_, hist_end, out_state.history_ + 1);
      ret.ngram_length = out_state.valid_length_ = in_state.valid_length_ + 1;
      // ret.prob was already set.
      return ret;
    }
    lookup_hash = CombineWordHash(lookup_hash, *hist_iter);
    if (mid_iter == middle_.end()) break;
    typename Middle::ConstIterator found;
    if (!mid_iter->Find(lookup_hash, found)) {
      // Didn't find an ngram using hist_iter.  
      // The history used in the found n-gram is [in_state.history_, hist_iter).  
      std::copy(in_state.history_, hist_iter, out_state.history_ + 1);
      // Therefore, we found a (hist_iter - in_state.history_ + 1)-gram including the last word.  
      ret.ngram_length = out_state.valid_length_ = (hist_iter - in_state.history_) + 1;
      ret.prob = std::accumulate(
          in_state.backoff_ + (mid_iter - middle_.begin()),
          in_state.backoff_ + in_state.valid_length_,
          ret.prob);
      return ret;
    }
    *backoff_out = found->GetValue().backoff;
    ret.prob = found->GetValue().prob;
  }
  
  typename Longest::ConstIterator found;
  if (!longest_.Find(lookup_hash, found)) {
    // It's an (P::Order()-1)-gram
    std::copy(in_state.history_, in_state.history_ + P::Order() - 2, out_state.history_ + 1);
    ret.ngram_length = out_state.valid_length_ = P::Order() - 1;
    ret.prob += in_state.backoff_[P::Order() - 2];
    return ret;
  }
  // It's an P::Order()-gram
  // out_state.valid_length_ is still P::Order() - 1 because the next lookup will only need that much.
  std::copy(in_state.history_, in_state.history_ + P::Order() - 2, out_state.history_ + 1);
  out_state.valid_length_ = P::Order() - 1;
  ret.ngram_length = P::Order();
  ret.prob = found->GetValue().prob;
  return ret;
}

template class GenericModel<ProbingSearch, MapVocabulary<ProbingSearch> >;
template class GenericModel<SortedUniformSearch, SortedVocabulary>;
} // namespace detail
} // namespace ngram
} // namespace lm
