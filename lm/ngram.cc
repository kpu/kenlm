#include "lm/ngram.hh"

#include "lm/arpa_io.hh"
#include "util/file_piece.hh"
#include "util/probing_hash_table.hh"
#include "util/scoped.hh"

#include <boost/lexical_cast.hpp>
#include <boost/progress.hpp>

#include <algorithm>
#include <functional>
#include <istream>
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

WordIndex Vocabulary::InsertUnique(std::string *word) {
  std::pair<boost::unordered_map<StringPiece, WordIndex>::const_iterator, bool> res(ids_.insert(std::make_pair(StringPiece(*word), available_)));
  if (__builtin_expect(!res.second, 0)) {
    delete word;
    throw WordDuplicateVocabLoadException(*word, res.first->second, available_);
  }
  strings_.push_back(word);
  return available_++;
}

void Vocabulary::FinishedLoading() {
  if (ids_.find(StringPiece("<s>")) == ids_.end()) throw BeginSentenceMissingException();
  if (ids_.find(StringPiece("</s>")) == ids_.end()) throw EndSentenceMissingException();
  // Allow lowercase form of unknown if found, otherwise complain.  It's better to not tolerate an   LM without OOV.   
  if (ids_.find(StringPiece("<unk>")) == ids_.end()) {
    if (ids_.find(StringPiece("<UNK>")) == ids_.end()) {
      // TODO: throw up unless there's a command line option saying not to.
      //throw UnknownMissingException();
      InsertUnique(new std::string("<unk>"));
    } else {
      ids_["<unk>"] = Index(StringPiece("<UNK>"));
    }
  }
  SetSpecial(Index(StringPiece("<s>")), Index(StringPiece("</s>")), Index(StringPiece("<unk>")), available_);
}

namespace detail {

struct VocabularyFriend {
  static void Reserve(Vocabulary &vocab, size_t to) {
    vocab.Reserve(to);
  }
  static WordIndex InsertUnique(Vocabulary &vocab, std::string *word) {
    return vocab.InsertUnique(word);
  }
  static void FinishedLoading(Vocabulary &vocab) {
    vocab.FinishedLoading();
  }
};

ImplBase::~ImplBase() {}

struct Prob {
  float prob;
  void SetBackoff(float to) {
    throw FormatLoadException("Attempt to set backoff " + boost::lexical_cast<std::string>(to) + " for an n-gram with longest order.");
  }
  void ZeroBackoff() {}
};
struct ProbBackoff : Prob {
  float backoff;
  void SetBackoff(float to) { backoff = to; }
  void ZeroBackoff() { backoff = 0.0; }
};

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

void Read1Grams(util::FilePiece &f, const size_t count, Vocabulary &vocab, ProbBackoff *unigrams) {
  ReadNGramHeader(f, 1);
  boost::progress_display progress(count, std::cerr, "Loading 1-grams\n");
  // +1 in case OOV is not found.
  detail::VocabularyFriend::Reserve(vocab, count + 1);
  // Special unigram reader because unigram's data structure is different and because we're inserting vocab words.
  std::auto_ptr<std::string> unigram(new std::string);
  for (size_t i = 0; i < count; ++i, ++progress) {
    float prob = f.ReadFloat();
    if (f.get() != '\t')
      throw FormatLoadException("Expected tab after probability");
    StringPiece unigram_piece(f.ReadDelimited());
    unigram->assign(unigram_piece.data(), unigram_piece.size());
    ProbBackoff &ent = unigrams[detail::VocabularyFriend::InsertUnique(vocab, unigram.release())];
    unigram.reset(new std::string);
    ent.prob = prob * M_LN10;
    char delim = f.get();
    if (delim == '\t') {
      ent.backoff = f.ReadFloat() * M_LN10;
      if ((f.get() != '\n')) throw FormatLoadException("Expected newline after backoff");
    } else if (delim == '\n') {
      ent.backoff = 0.0;
    } else {
      ent.backoff = 0.0;
      throw FormatLoadException("Expected tab or newline after unigram");
    }
  }
  if (f.ReadLine().size()) throw FormatLoadException("Blank line after ngrams not blank");
  detail::VocabularyFriend::FinishedLoading(vocab);
}

template <class Store> void ReadNGrams(util::FilePiece &f, const unsigned int n, const size_t count, const Vocabulary &vocab, Store &store) {
  boost::progress_display progress(count, std::cerr, std::string("Loading ") + boost::lexical_cast<std::string>(n) + "-grams\n");

  ReadNGramHeader(f, n);

  // vocab ids of words in reverse order
  WordIndex vocab_ids[n];
  typename Store::Value value;
  for (size_t i = 0; i < count; ++i) {
    try {
      value.prob = f.ReadFloat() * M_LN10;
      for (WordIndex *vocab_out = &vocab_ids[n-1]; vocab_out >= vocab_ids; --vocab_out) {
        *vocab_out = vocab.Index(f.ReadDelimited());
      }
      uint64_t key = ChainedWordHash(vocab_ids, vocab_ids + n);

      switch (f.get()) {
        case '\t':
          value.SetBackoff(f.ReadFloat() * M_LN10);
          break;
        case '\n':
          value.ZeroBackoff();
          break;
        default:
          throw FormatLoadException("Got unexpected delimiter before backoff weight");
      }
      store.Insert(key, value);
      ++progress;
    } catch (const std::exception &f) {
      throw FormatLoadException("Error reading the " + boost::lexical_cast<std::string>(i) + "th " + boost::lexical_cast<std::string>(n) + "-gram." + f.what());
    }
  }

  if (f.ReadLine().size()) throw FormatLoadException("Blank line after ngrams not blank");
}

// Should return the same results as SRI except ln instead of log10
template <class Search> class GenericModel : public ImplBase {
  public:
    GenericModel(const char *file, Vocabulary &vocab, const typename Search::Init &init, unsigned int &order, State &begin_sentence);

    Return IncrementalScore(
        const State &in_state,
        const WordIndex new_word,
        State &out_state) const;

  private:
    void LoadFromARPA(util::FilePiece &f, Vocabulary &vocab, const std::vector<size_t> &counts);

    unsigned int order_;

    WordIndex not_found_;

    // memory_ is the backing store for unigram_, [middle_begin_, middle_end_), and longest_.  All of these are pointers there.   
    util::scoped_mmap memory_;

    ProbBackoff *unigram_;

    typedef typename Search::template Table<ProbBackoff>::T Middle;
    std::vector<Middle> middle_;

    typedef typename Search::template Table<Prob>::T Longest;
    Longest longest_;
};

template <class Search> GenericModel<Search>::GenericModel(const char *file, Vocabulary &vocab, const typename Search::Init &search_init, unsigned int &order, State &begin_sentence) {
  util::FilePiece f(file);

  std::vector<size_t> counts;
  ReadCounts(f, counts);

  if (counts.size() < 2)
    throw FormatLoadException("This ngram implementation assumes at least a bigram model.");
  if (counts.size() > kMaxOrder)
    throw FormatLoadException(std::string("Edit ngram.hh and change kMaxOrder to at least ") + boost::lexical_cast<std::string>(counts.size()));
  order_ = counts.size();

  size_t memory_size = sizeof(ProbBackoff) * counts[0];
  for (unsigned int n = 2; n < order_; ++n) {
    memory_size += Middle::Size(search_init, counts[n-1]);
  }
  memory_size += Longest::Size(search_init, counts[order_ - 1]);
  memory_.reset(mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0), memory_size);
  if (memory_.get() == MAP_FAILED) throw AllocateMemoryLoadException(memory_size, errno);

  unigram_ = reinterpret_cast<ProbBackoff*>(memory_.get());
  char *start = reinterpret_cast<char *>(memory_.get()) + sizeof(ProbBackoff) * counts[0];
  for (unsigned int n = 2; n < order_; ++n) {
    middle_.push_back(Middle(search_init, start, counts[n - 1]));
    start += Middle::Size(search_init, counts[n - 1]);
  }
  longest_ = Longest(search_init, start, counts[order_ - 1]);
  assert(start + Longest::Size(search_init, counts[order_ - 1]) - reinterpret_cast<char*>(memory_.get()) == memory_size);

  LoadFromARPA(f, vocab, counts);

  if (std::fabs(unigram_[vocab.NotFound()].backoff) > 0.0000001) {
    throw FormatLoadException(std::string("Backoff for unknown word with index ") + boost::lexical_cast<std::string>(vocab.NotFound()) + " is " + boost::lexical_cast<std::string>(unigram_[vocab.NotFound()].backoff) + std::string(" not zero"));
  }
  not_found_ = vocab.NotFound();
  order = order_;
  begin_sentence.valid_length_ = 1;
  begin_sentence.history_[0] = vocab.BeginSentence();
  begin_sentence.backoff_[0] = unigram_[begin_sentence.history_[0]].backoff;
}

template <class Search> void GenericModel<Search>::LoadFromARPA(util::FilePiece &f, Vocabulary &vocab, const std::vector<size_t> &counts) {
  // Read the unigrams.
  Read1Grams(f, counts[0], vocab, unigram_);
  
  // Read the n-grams.
  for (unsigned int n = 2; n < counts.size(); ++n) {
    ReadNGrams<Middle>(f, n, counts[n-1], vocab, middle_[n-2]);
  }
  ReadNGrams<Longest>(f, counts.size(), counts[counts.size() - 1], vocab, longest_);
}

/* Ugly optimized function.
 * in_state contains the previous ngram's length and backoff probabilites to
 * be used here.  out_state is populated with the found ngram length and
 * backoffs that the next call will find useful.  
 *
 * The search goes in increasing order of ngram length.  
 */
template <class Search> Return GenericModel<Search>::IncrementalScore(
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
    // It's an (order_-1)-gram
    std::copy(in_state.history_, in_state.history_ + order_ - 2, out_state.history_ + 1);
    ret.ngram_length = out_state.valid_length_ = order_ - 1;
    ret.prob += in_state.backoff_[order_ - 2];
    return ret;
  }
  // It's an order_-gram
  // out_state.valid_length_ is still order_ - 1 because the next lookup will only need that much.
  std::copy(in_state.history_, in_state.history_ + order_ - 2, out_state.history_ + 1);
  out_state.valid_length_ = order_ - 1;
  ret.ngram_length = order_;
  ret.prob = found->prob;
  return ret;
}

class ProbingSearch {
  private:
    // std::identity is an SGI extension :-(
    struct IdentityHash : public std::unary_function<uint64_t, size_t> {
      size_t operator()(uint64_t arg) const { return static_cast<size_t>(arg); }
    };

  public:
    typedef float Init;
    template <class Value> struct Table {
      typedef util::ProbingTable<uint64_t, Value, IdentityHash> T;
    };
};

} // namespace detail

Model::Model(const char *file) {
  impl_.reset(new detail::GenericModel<detail::ProbingSearch>(file, vocab_, 1.5, order_, begin_sentence_));
  null_context_.valid_length_ = 0;
}

Model::~Model() {}

} // namespace ngram
} // namespace lm
