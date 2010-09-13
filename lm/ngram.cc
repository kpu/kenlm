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

std::size_t SortedVocabulary::Size(std::size_t entries, float ignored) {
  // Lead with the number of entries.  
  return sizeof(uint64_t) + sizeof(Entry) * entries;
}

void SortedVocabulary::Init(void *start, std::size_t allocated, std::size_t entries) {
  assert(allocated >= Size(entries));
  // Leave space for number of entries.  
  begin_ = reinterpret_cast<Entry*>(reinterpret_cast<uint64_t*>(start) + 1);
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

bool SortedVocabulary::FinishedLoading(detail::ProbBackoff *reorder_vocab) {
  util::JointSort(begin_, end_, reorder_vocab + 1);
  SetSpecial(Index("<s>"), Index("</s>"), 0, end_ - begin_ + 1);
  // Save size.  
  *(reinterpret_cast<uint64_t*>(begin_) - 1) = end_ - begin_;
  return saw_unk_;
}

void SortedVocabulary::LoadedBinary() {
  end_ = begin_ + *(reinterpret_cast<const uint64_t*>(begin_) - 1);
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

template <class Search> bool MapVocabulary<Search>::FinishedLoading(ProbBackoff *reorder_vocab) {
  lookup_.FinishedInserting();
  SetSpecial(Index("<s>"), Index("</s>"), 0, available_);
  return saw_unk_;
}

template <class Search> void MapVocabulary<Search>::LoadedBinary() {
  lookup_.LoadedBinary();
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
  if (counts.size() > kMaxOrder) UTIL_THROW(FormatLoadException, "This model has order " << counts.size() << ".  Edit ngram.hh's kMaxOrder to at least this value and recompile.");
  if (counts.size() < 2) UTIL_THROW(FormatLoadException, "This ngram implementation assumes at least a bigram model.");
  size_t memory_size = VocabularyT::Size(counts[0], config.probing_multiplier);
  memory_size += sizeof(ProbBackoff) * (counts[0] + 1); // +1 for hallucinate <unk>
  for (unsigned char n = 2; n < counts.size(); ++n) {
    memory_size += Middle::Size(counts[n - 1], config.probing_multiplier);
  }
  memory_size += Longest::Size(counts.back(), config.probing_multiplier);
  return memory_size;
}

template <class Search, class VocabularyT> void GenericModel<Search, VocabularyT>::SetupMemory(char *base, const std::vector<size_t> &counts, const Config &config) {
  char *start = base;
  size_t allocated = VocabularyT::Size(counts[0], config.probing_multiplier);
  vocab_.Init(start, allocated, counts[0]);
  start += allocated;
  unigram_ = reinterpret_cast<ProbBackoff*>(start);
  start += sizeof(ProbBackoff) * (counts[0] + 1);
  for (unsigned int n = 2; n < counts.size(); ++n) {
    allocated = Middle::Size(counts[n - 1], config.probing_multiplier);
    middle_.push_back(Middle(start, allocated));
    start += allocated;
  }
  allocated = Longest::Size(counts.back(), config.probing_multiplier);
  longest_ = Longest(start, allocated);
  start += allocated;
  if (static_cast<std::size_t>(start - base) != Size(counts, config)) UTIL_THROW(FormatLoadException, "The data structures took " << (start - base) << " but Size says they should take " << Size(counts, config));
}

const char kMagicBytes[] = "mmap lm http://kheafield.com/code format version 0\n\0";
struct BinaryFileHeader {
  char magic[sizeof(kMagicBytes)];
  float zero_f, one_f, minus_half_f;
  WordIndex one_word_index, max_word_index;
  uint64_t one_uint64;

  void SetToReference() {
    std::memcpy(magic, kMagicBytes, sizeof(magic));
    zero_f = 0.0; one_f = 1.0; minus_half_f = -0.5;
    one_word_index = 1;
    max_word_index = std::numeric_limits<WordIndex>::max();
    one_uint64 = 1;
  }
};

bool IsBinaryFormat(int fd, off_t size) {
  if (size == util::kBadSize || (size <= static_cast<off_t>(sizeof(BinaryFileHeader)))) return false;
  // Try reading the header.  
  util::scoped_mmap memory(mmap(NULL, sizeof(BinaryFileHeader), PROT_READ, MAP_FILE | MAP_PRIVATE, fd, 0), sizeof(BinaryFileHeader));
  if (memory.get() == MAP_FAILED) return false;
  BinaryFileHeader reference_header = BinaryFileHeader();
  reference_header.SetToReference();
  if (!memcmp(memory.get(), &reference_header, sizeof(BinaryFileHeader))) return true;
  if (!memcmp(memory.get(), "mmap lm ", 8)) UTIL_THROW(FormatLoadException, "File looks like it should be loaded with mmap, but the test values don't match.  Was it built on a different machine or with a different compiler?");
  return false;
}

std::size_t BinaryCountsSize(unsigned char order) {
  return 1 + sizeof(uint64_t) * order;
}

std::size_t Align8(std::size_t in) {
  std::size_t off = in % 8;
  if (!off) return in;
  return in + 8 - off;
}

std::size_t TotalHeaderSize(unsigned int order) {
  return Align8(sizeof(BinaryFileHeader) + BinaryCountsSize(order));;
}

const char *ReadBinaryCounts(off_t remaining, const void *from, std::vector<size_t> &out) {
  const char *from_char = reinterpret_cast<const char*>(from);
  if (remaining < 1) UTIL_THROW(FormatLoadException, "File too short to have count information.");
  unsigned char order = *reinterpret_cast<const unsigned char*>(from_char);
  if (remaining + 1 < static_cast<off_t>(sizeof(uint64_t) * order)) UTIL_THROW(FormatLoadException, "File too short to have count information.");
  out.resize(static_cast<std::size_t>(order));
  const uint64_t *counts = reinterpret_cast<const uint64_t*>(from_char + 1);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::size_t>(counts[i]);
  }
  return from_char + BinaryCountsSize(out.size());
}

void WriteBinaryHeader(void *to, const std::vector<size_t> &from) {
  BinaryFileHeader header = BinaryFileHeader();
  header.SetToReference();
  memcpy(to, &header, sizeof(BinaryFileHeader));
  char *out = reinterpret_cast<char*>(to) + sizeof(BinaryFileHeader);
  *reinterpret_cast<unsigned char*>(out) = static_cast<unsigned char>(from.size());
  uint64_t *counts = reinterpret_cast<uint64_t*>(out + 1);
  for (std::size_t i = 0; i < from.size(); ++i) {
    counts[i] = from[i];
  }
}

template <class Search, class VocabularyT> GenericModel<Search, VocabularyT>::GenericModel(const char *file, const Config &config) : mapped_file_(util::OpenReadOrThrow(file)) {
  const off_t file_size = util::SizeFile(mapped_file_.get());

  std::vector<size_t> counts;

  if (IsBinaryFormat(mapped_file_.get(), file_size)) {
    memory_.reset(mmap(NULL, file_size, PROT_READ, MAP_FILE | MAP_PRIVATE, mapped_file_.get(), 0), file_size);
    if (MAP_FAILED == memory_.get()) UTIL_THROW(util::ErrnoException, "Couldn't mmap the whole " << file);

    ReadBinaryCounts(file_size - sizeof(BinaryFileHeader), memory_.begin() + sizeof(BinaryFileHeader), counts);
    size_t memory_size = Size(counts, config);

    char *start = reinterpret_cast<char*>(memory_.get()) + TotalHeaderSize(counts.size());
    if (memory_size != static_cast<size_t>(memory_.end() - start)) UTIL_THROW(FormatLoadException, "The mmap file " << file << " has size " << file_size << " but " << (memory_size + TotalHeaderSize(counts.size())) << " was expected based on the number of counts and configuration.");

    SetupMemory(start, counts, config);
    vocab_.LoadedBinary();
    for (typename std::vector<Middle>::iterator i = middle_.begin(); i != middle_.end(); ++i) {
      i->LoadedBinary();
    }
    longest_.LoadedBinary();

  } else {

    util::FilePiece f(file, mapped_file_.release(), config.messages);
    ReadCounts(f, counts);
    size_t memory_size = Size(counts, config);
    char *start;

    if (config.write_mmap) {
      // Write out an mmap file.  
      // O_TRUNC insures that the later ftruncate call fills with zeros.  The data structures like being initialized with zeros.  
      mapped_file_.reset(open(config.write_mmap, O_CREAT | O_RDWR | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH));
      if (-1 == mapped_file_.get()) UTIL_THROW(util::ErrnoException, "Couldn't create " << config.write_mmap);
      size_t total_size = TotalHeaderSize(counts.size()) + memory_size;
      if (-1 == ftruncate(mapped_file_.get(), total_size)) UTIL_THROW(util::ErrnoException, "ftruncate on " << config.write_mmap << " to " << total_size << " failed.");
      memory_.reset(mmap(NULL, total_size, PROT_READ | PROT_WRITE, MAP_FILE | MAP_SHARED, mapped_file_.get(), 0), total_size);
      if (memory_.get() == MAP_FAILED) UTIL_THROW(util::ErrnoException, "Failed to mmap " << config.write_mmap);
      WriteBinaryHeader(memory_.get(), counts);
      start = reinterpret_cast<char*>(memory_.get()) + TotalHeaderSize(counts.size());
    } else {
      memory_.reset(mmap(NULL, memory_size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0), memory_size);
      if (memory_.get() == MAP_FAILED) throw AllocateMemoryLoadException(memory_size);
      start = reinterpret_cast<char*>(memory_.get());
    }
    SetupMemory(start, counts, config);
    try {
      LoadFromARPA(f, counts, config);
    } catch (FormatLoadException &e) {
      e << " in file " << file;
      throw;
    }
  }

  // g++ prints warnings unless these are fully initialized.  
  State begin_sentence = State();
  begin_sentence.valid_length_ = 1;
  begin_sentence.history_[0] = vocab_.BeginSentence();
  begin_sentence.backoff_[0] = unigram_[begin_sentence.history_[0]].backoff;
  State null_context = State();
  null_context.valid_length_ = 0;
  P::Init(begin_sentence, null_context, vocab_, counts.size());
}

template <class Search, class VocabularyT> void GenericModel<Search, VocabularyT>::LoadFromARPA(util::FilePiece &f, const std::vector<size_t> &counts, const Config &config) {
  // Read the unigrams.
  Read1Grams(f, counts[0], vocab_, unigram_);
  bool saw_unk = vocab_.FinishedLoading(unigram_);
  if (!saw_unk) {
    switch(config.unknown_missing) {
      case Config::THROW_UP:
        {
          SpecialWordMissingException e("<unk>");
          e << " and configuration was set to throw if unknown is missing";
          throw e;
        }
      case Config::COMPLAIN:
        if (config.messages) *config.messages << "Language model is missing <unk>.  Substituting probability " << config.unknown_missing_prob << "." << std::endl; 
        // There's no break;.  This is by design.  
      case Config::SILENT:
        // Default probabilities for unknown.  
        unigram_[0].backoff = 0.0;
        unigram_[0].prob = config.unknown_missing_prob;
        break;
    }
  }
  
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
