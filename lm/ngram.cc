#include "lm/arpa_io.hh"
#include "lm/ngram.hh"
#include "util/probing_hash_table.hh"

#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/progress.hpp>

#include <algorithm>
#include <istream>
#include <numeric>
#include <string>

#include <cmath>

#include <sys/mman.h>
#include <errno.h>
#include <stdlib.h>

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
class Mapped {
  public:
    Mapped() : data_(MAP_FAILED) {}
    Mapped(void *data, size_t size) : data_(data), size_(size) {}
    ~Mapped() { reset(); }

    void *get() { return data_; }

    void reset(void *data, size_t size) {
      reset();
      data_ = data;
      size_ = size;
    }

    void reset() {
      if (data_ != MAP_FAILED) {
        if (munmap(data_, size_))
          err(1, "Couldn't munmap language model memory");
        data_ = MAP_FAILED;
      }
    }

  private:
    void *data_;
    size_t size_;
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

void Read1Grams(std::istream &f, const size_t count, Vocabulary &vocab, ProbBackoff *unigrams) {
  ReadNGramHeader(f, 1);
  boost::progress_display progress(count, std::cerr, "Loading 1-grams\n");
  // +1 in case OOV is not found.
  detail::VocabularyFriend::Reserve(vocab, count + 1);
  std::string line;
  // Special unigram reader because unigram's data structure is different and because we're inserting vocab words.
  std::auto_ptr<std::string> unigram(new std::string);
  for (size_t i = 0; i < count; ++i) {
    float prob;
    f >> prob;
    if (f.get() != '\t')
      throw FormatLoadException("Expected tab after probability");
    f >> *unigram;
    if (!f) throw FormatLoadException("Actual unigram count less than reported");
    ProbBackoff &ent = unigrams[detail::VocabularyFriend::InsertUnique(vocab, unigram.release())];
    unigram.reset(new std::string);
    ent.prob = prob * M_LN10;
    int delim = f.get();
    if (!f) throw FormatLoadException("Missing line termination while reading unigrams");
    if (delim == '\t') {
      if (!(f >> ent.backoff)) throw FormatLoadException("Failed to read backoff");
      ent.backoff *= M_LN10;
      if ((f.get() != '\n') || !f) throw FormatLoadException("Expected newline after backoff");
    } else if (delim == '\n') {
      ent.backoff = 0.0;
    } else {
      ent.backoff = 0.0;
      throw FormatLoadException("Expected tab or newline after unigram");
    }
    ++progress;
  }
  if (getline(f, line)) FormatLoadException("Blank line after ngrams missing");
  if (!line.empty()) throw FormatLoadException("Blank line after ngrams not blank", line);
  detail::VocabularyFriend::FinishedLoading(vocab);
}

template <class Store> void ReadNGrams(std::istream &f, const unsigned int n, const size_t count, const Vocabulary &vocab, Store &store) {
  boost::progress_display progress(count, std::cerr, std::string("Loading ") + boost::lexical_cast<std::string>(n) + "-grams\n");

  ReadNGramHeader(f, n);

  // vocab ids of words in reverse order
  WordIndex vocab_ids[n];
  std::string word;
  typename Store::Value value;
  for (size_t i = 0; i < count; ++i) {
    try {
      f >> value.prob;
      value.prob *= M_LN10;
      for (WordIndex *vocab_out = &vocab_ids[n-1]; vocab_out >= vocab_ids; --vocab_out) {
        f >> word;
        *vocab_out = vocab.Index(word);
      }
      uint64_t key = ChainedWordHash(vocab_ids, vocab_ids + n);

      switch (f.get()) {
        case '\t':
          float backoff;
          f >> backoff;
          value.SetBackoff(backoff * M_LN10);
          break;
        case '\n':
          value.ZeroBackoff();
          break;
        default:
          throw FormatLoadException("Got unexpected delimiter before backoff weight");
      }
      store.Insert(key, value);
      ++progress;
    } catch (const std::ios_base::failure &f) {
      throw FormatLoadException("Error reading the " + boost::lexical_cast<std::string>(i) + "th " + boost::lexical_cast<std::string>(n) + "-gram.");
    }
  }

  std::string line;
  if (!getline(f, line)) FormatLoadException("Blank line after ngrams missing");
  if (!line.empty()) throw FormatLoadException("Blank line after ngrams not blank", line);
}

// Should return the same results as SRI except ln instead of log10
template <class Search> class GenericModel : public ImplBase {
  public:
    GenericModel(const char *file, Vocabulary &vocab, const typename Search::Init &init, unsigned int &order, float &begin_backoff);

    float IncrementalScore(
        const State &in_state,
        const WordIndex *const words,
        const WordIndex *const words_end,
        State &out_state) const;

  private:
    void LoadFromARPA(std::istream &f, Vocabulary &vocab, const std::vector<size_t> &counts);

    unsigned int order_;

    WordIndex not_found_;

    // memory_ is the backing store for unigram_, [middle_begin_, middle_end_), and longest_.  All of these are pointers there.   
    Mapped memory_;

    ProbBackoff *unigram_;

    typedef typename Search::template Table<uint64_t, ProbBackoff> Middle;
    std::vector<Middle> middle_;

    typedef typename Search::template Table<uint64_t, Prob> Longest;
    Longest longest_;
};

template <class Search> GenericModel<Search>::GenericModel(const char *file, Vocabulary &vocab, const typename Search::Init &search_init, unsigned int &order, float &begin_backoff) {
  boost::iostreams::stream<boost::iostreams::mapped_file_source> f(file);
  if (!f) throw OpenFileLoadException(file);
  f.exceptions(std::istream::failbit | std::istream::badbit);

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
  begin_backoff = unigram_[vocab.BeginSentence()].backoff;
}

template <class Search> void GenericModel<Search>::LoadFromARPA(std::istream &f, Vocabulary &vocab, const std::vector<size_t> &counts) {
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
template <class Search> float GenericModel<Search>::IncrementalScore(
    const State &in_state,
    const WordIndex *const words_begin,
    const WordIndex *const words_end,
    State &out_state) const {
  assert(words_end > words_begin);

  // This is end pointer passed to SumBackoffs.
  const ProbBackoff &unigram = unigram_[*words_begin];
  if (*words_begin == not_found_) {
    out_state.ngram_length_ = 0;
    // all of backoff.
    return std::accumulate(
        in_state.backoff_.data(),
        in_state.backoff_.data() + std::min<unsigned int>(in_state.NGramLength(), order_ - 1),
        unigram.prob);
  }
  boost::array<float, kMaxOrder - 1>::iterator backoff_out(out_state.backoff_.begin());
  *backoff_out = unigram.backoff;
  if (in_state.NGramLength() == 0) {
    out_state.ngram_length_ = 1;
    // No backoff because NGramLength() == 0 and unknown can't have backoff.
    return unigram.prob;
  }
  ++backoff_out;

  // Ok now we now that the bigram contains known words.  Start by looking it up.
  
  float prob = unigram.prob;
  uint64_t lookup_hash = static_cast<uint64_t>(*words_begin);
  const WordIndex *words_iter = words_begin + 1;
  typename std::vector<Middle>::const_iterator mid_iter = middle_.begin();
  for (; ; ++mid_iter, ++words_iter, ++backoff_out) {
    if (words_iter == words_end) {
      // ran out of words, so there shouldn't be backoff.
      out_state.ngram_length_ = (words_iter - words_begin);
      return prob;
    }
    lookup_hash = CombineWordHash(lookup_hash, *words_iter);
    if (mid_iter == middle_.end()) break;
    const ProbBackoff *found;
    if (!mid_iter->Find(lookup_hash, found)) {
      // Found an ngram of length words_iter - words_begin, but not of length words_iter - words_begin + 1.
      // Sum up backoffs for histories of length
      //   [words_iter - words_begin, std::min(in_state.NGramLength(), order_ - 1)).
      // These correspond to
      //   &in_state.backoff_[words_iter - words_begin - 1] to ending_backoff
      // which is the same as
      //   &in_state.backoff_[(mid_iter - middle_begin)] to ending_backoff.
      out_state.ngram_length_ = (words_iter - words_begin);
      return std::accumulate(
          in_state.backoff_.data() + (mid_iter - middle_.begin()), 
          in_state.backoff_.data() + std::min<unsigned int>(in_state.NGramLength(), order_ - 1),
          prob);
    }
    *backoff_out = found->backoff;
    prob = found->prob;
  }
  
  const Prob *found;
  if (!longest_.Find(lookup_hash, found)) {
    // It's an (order_-1)-gram
    out_state.ngram_length_ = order_ - 1;
    return prob + in_state.backoff_[order_ - 1 - 1];
  }
  // It's an order_-gram
  out_state.ngram_length_ = order_;
  if (order_ < kMaxOrder) {
    // In this case, State hashing and equality will check ngram_length_ entries.
    // However, the last entry is not a valid backoff weight, so here it is set
    // to 0.0.  Specifically
    // order_ - 1 = min(out_state.ngram_length_, order_ - 1) = min(out_state.ngram_length_, kMaxOrder - 1) - 1 = out_state.ngram_length_ - 1
    out_state.backoff_[order_ - 1] = 0.0;
  }
  return found->prob;  
}

struct ProbingSearch {
  typedef float Init;
  template <class KeyT, class ValueT> class Table {
    public:
      typedef KeyT Key;
      typedef ValueT Value;

      static size_t Size(float multiplier, size_t entries) {
        return static_cast<size_t>(multiplier * static_cast<float>(entries)) * sizeof(Entry);
      }

      Table() {}

      Table(float multiplier, char *start, size_t entries) 
        : table_(
            reinterpret_cast<Entry*>(start),
            static_cast<size_t>(multiplier * static_cast<float>(entries)),
            Entry(),
            IdentityKey(),
            EqualsKeyOnly()) {}

      bool Find(const Key &key, const Value *&value) const {
        const Entry *e = table_.Find(key);
        if (!e) return false;
        value = &e->value;
        return true;
      }
      void Insert(const Key &key, const Value &value) {
        Entry e;
        e.key = key;
        e.value = value;
        table_.Insert(e);
      }

    private:
      struct Entry {
        Key key;
        Value value;
      };
      struct IdentityKey : public std::unary_function<const Entry &, size_t> {
        size_t operator()(const Entry &e) const { return e.key; }
        size_t operator()(const Key value) const { return value; }
      };
      struct EqualsKeyOnly : public std::binary_function<const Entry &, const Entry &, bool> {
        bool operator()(const Entry &a, const Entry &b) const {
          return a.key == b.key;
        }
        bool operator()(const Entry &a, const Key k) const {
          return a.key == k;
        }
      };

      util::ProbingHashTable<Entry, IdentityKey, EqualsKeyOnly> table_;
  };
};

} // namespace detail

Model::Model(const char *file) {
  impl_.reset(new detail::GenericModel<detail::ProbingSearch>(file, vocab_, 1.5, order_, begin_sentence_.backoff_[0]));
  begin_sentence_.ngram_length_ = 1;
  null_context_.ngram_length_ = 0;
}

Model::~Model() {}

} // namespace ngram
} // namespace lm
