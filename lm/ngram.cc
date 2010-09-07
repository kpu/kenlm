#include "lm/ngram.hh"

#include "lm/arpa_io.hh"
#include "util/file_piece.hh"
#include "util/probing_hash_table.hh"
#include "util/scoped.hh"

#include <boost/lexical_cast.hpp>
#include <boost/progress.hpp>

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

template <class Search> void GenericVocabulary<Search>::Init(const typename Search::Init &search_init, char *start, std::size_t entries) {
  lookup_ = Lookup(search_init, start, entries);
}

template <class Search> WordIndex GenericVocabulary<Search>::Insert(const StringPiece &str) {
  lookup_.Insert(Hash(str), next_);
  return next_++;
}

template <class Search> void GenericVocabulary<Search>::FinishedLoading() {
  lookup_.FinishedInserting();
  WordIndex begin, end, unk;
  if (!Find("<s>", begin)) throw BeginSentenceMissingException();
  if (!Find("</s>", end)) throw EndSentenceMissingException();
  if (!Find("<unk>", unk)) {
    if (Find("<UNK>", unk)) {
      uint64_t new_key = Hash("<unk>");
      lookup_.Insert(new_key, unk);
    } else {
      // TODO: command line option to not throw up.  
      throw UnknownMissingException();
    }
  }
  SetSpecial(begin, end, unk, next_);
}

template <class Search> bool GenericVocabulary<Search>::Find(const StringPiece &str, WordIndex &found) {
  const WordIndex *pointer;
  bool ret = lookup_.Find(Hash(str), pointer);
  found = *pointer;
  return ret;
}

// All of the entropy is in low order bits and boost::hash does poorly with these.
// Odd numbers near 2^64 chosen by mashing on the keyboard.  
inline uint64_t CombineWordHash(uint64_t current, const uint32_t next) {
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
  boost::progress_display progress(count, std::cerr, "Loading 1-grams\n");
  for (size_t i = 0; i < count; ++i, ++progress) {
    try {
      float prob = f.ReadFloat();
      if (f.get() != '\t')
        throw FormatLoadException("Expected tab after probability");
      ProbBackoff &value = unigrams[vocab.Insert(f.ReadDelimited())];
      value.prob = prob;
      switch (f.get()) {
        case '\t':
          value.SetBackoff(f.ReadFloat());
          if ((f.get() != '\n')) throw FormatLoadException("Expected newline after backoff");
          break;
        case '\n':
          value.ZeroBackoff();
          break;
        default:
          throw FormatLoadException("Expected tab or newline after unigram");
      }
    } catch (const std::exception &f) {
      throw FormatLoadException("Error reading the " + boost::lexical_cast<std::string>(i) + "th 1-gram.  " + f.what());
    }
  }
  if (f.ReadLine().size()) throw FormatLoadException("Blank line after ngrams not blank");
  vocab.FinishedLoading();
}

template <class Voc, class Store> void ReadNGrams(util::FilePiece &f, const unsigned int n, const size_t count, const Voc &vocab, Store &store) {
  ReadNGramHeader(f, n);
  boost::progress_display progress(count, std::cerr, std::string("Loading ") + boost::lexical_cast<std::string>(n) + "-grams\n");

  // vocab ids of words in reverse order
  WordIndex vocab_ids[n];
  typename Store::Value value;
  for (size_t i = 0; i < count; ++i, ++progress) {
    try {
      value.prob = f.ReadFloat();
      for (WordIndex *vocab_out = &vocab_ids[n-1]; vocab_out >= vocab_ids; --vocab_out) {
        *vocab_out = vocab.Index(f.ReadDelimited());
      }
      uint64_t key = ChainedWordHash(vocab_ids, vocab_ids + n);

      switch (f.get()) {
        case '\t':
          value.SetBackoff(f.ReadFloat());
          break;
        case '\n':
          value.ZeroBackoff();
          break;
        default:
          throw FormatLoadException("Got unexpected delimiter before backoff weight");
      }
      store.Insert(key, value);
    } catch (const std::exception &f) {
      throw FormatLoadException("Error reading the " + boost::lexical_cast<std::string>(i) + "th " + boost::lexical_cast<std::string>(n) + "-gram." + f.what());
    }
  }

  if (f.ReadLine().size()) throw FormatLoadException("Blank line after ngrams not blank");
  store.FinishedInserting();
}

void Prob::SetBackoff(float to) {
  throw FormatLoadException("Attempt to set backoff " + boost::lexical_cast<std::string>(to) + " for an n-gram with longest order.");
}

template <class Search> size_t GenericModel<Search>::Size(const typename Search::Init &search_init, const std::vector<size_t> &counts) {
  if (counts.size() < 2)
    throw FormatLoadException("This ngram implementation assumes at least a bigram model.");
  size_t memory_size = GenericVocabulary<Search>::Size(search_init, counts[0]);
  memory_size += sizeof(ProbBackoff) * counts[0];
  for (unsigned char n = 2; n < counts.size(); ++n) {
    memory_size += Middle::Size(search_init, counts[n - 1]);
  }
  memory_size += Longest::Size(search_init, counts.back());
  return memory_size;
}

template <class Search> GenericModel<Search>::GenericModel(const char *file, const typename Search::Init &search_init) {
  util::FilePiece f(file);

  std::vector<size_t> counts;
  ReadCounts(f, counts);

  if (counts.size() < 2)
    throw FormatLoadException("This ngram implementation assumes at least a bigram model.");
  if (counts.size() > kMaxOrder)
    throw FormatLoadException(std::string("Edit ngram.hh and change kMaxOrder to at least ") + boost::lexical_cast<std::string>(counts.size()));
  unsigned char order = counts.size();

  const size_t memory_size = Size(search_init, counts);
  memory_.reset(mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0), memory_size);
  if (memory_.get() == MAP_FAILED) throw AllocateMemoryLoadException(memory_size, errno);

  char *start = static_cast<char*>(memory_.get());
  vocab_.Init(search_init, start, counts[0]);
  start += GenericVocabulary<Search>::Size(search_init, counts[0]);
  unigram_ = reinterpret_cast<ProbBackoff*>(start);
  start += sizeof(ProbBackoff) * counts[0];
  for (unsigned int n = 2; n < order; ++n) {
    middle_.push_back(Middle(search_init, start, counts[n - 1]));
    start += Middle::Size(search_init, counts[n - 1]);
  }
  longest_ = Longest(search_init, start, counts[order - 1]);
  assert(static_cast<size_t>(start + Longest::Size(search_init, counts[order - 1]) - reinterpret_cast<char*>(memory_.get())) == memory_size);

  LoadFromARPA(f, counts);

  not_found_ = vocab_.NotFound();
  if (std::fabs(unigram_[not_found_].backoff) > 0.0000001) {
    throw FormatLoadException(std::string("Backoff for unknown word with index ") + boost::lexical_cast<std::string>(not_found_) + " is " + boost::lexical_cast<std::string>(unigram_[not_found_].backoff) + std::string(" not zero"));
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

template <class Search> void GenericModel<Search>::LoadFromARPA(util::FilePiece &f, const std::vector<size_t> &counts) {
  // Read the unigrams.
  Read1Grams(f, counts[0], vocab_, unigram_);
  
  // Read the n-grams.
  for (unsigned int n = 2; n < counts.size(); ++n) {
    ReadNGrams(f, n, counts[n-1], vocab_, middle_[n-2]);
  }
  ReadNGrams(f, counts.size(), counts[counts.size() - 1], vocab_, longest_);
}

/* Ugly optimized function.
 * in_state contains the previous ngram's length and backoff probabilites to
 * be used here.  out_state is populated with the found ngram length and
 * backoffs that the next call will find useful.  
 *
 * The search goes in increasing order of ngram length.  
 */
template <class Search> Return GenericModel<Search>::WithLength(
    const State &in_state,
    const WordIndex new_word,
    State &out_state) const {

  Return ret;
  // This is end pointer passed to SumBackoffs.
  const ProbBackoff &unigram = unigram_[new_word];
  if (new_word == not_found_) {
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
    const ProbBackoff *found;
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
    *backoff_out = found->backoff;
    ret.prob = found->prob;
  }
  
  const Prob *found;
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
  ret.prob = found->prob;
  return ret;
}

// This also instantiates GenericVocabulary.
template class GenericModel<ProbingSearch>;
template class GenericModel<SortedUniformSearch>;
} // namespace detail
} // namespace ngram
} // namespace lm
