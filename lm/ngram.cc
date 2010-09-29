#include "lm/ngram.hh"

#include "lm/exception.hh"
#include "lm/read_arpa.hh"
#include "util/file_piece.hh"
#include "util/joint_sort.hh"
#include "util/murmur_hash.hh"
#include "util/probing_hash_table.hh"

#include <algorithm>
#include <functional>
#include <numeric>
#include <limits>
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

size_t hash_value(const State &state) {
  return util::MurmurHashNative(state.history_, sizeof(WordIndex) * state.valid_length_);
}

namespace {

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

template <class Voc, class Store> void ReadNGrams(util::FilePiece &f, const unsigned int n, const size_t count, const Voc &vocab, Store &store) {
  ReadNGramHeader(f, n);

  // vocab ids of words in reverse order
  WordIndex vocab_ids[n];
  typename Store::Packing::Value value;
  for (size_t i = 0; i < count; ++i) {
    ReadNGram(f, n, vocab, vocab_ids, value);
    uint64_t key = ChainedWordHash(vocab_ids, vocab_ids + n);
    store.Insert(Store::Packing::Make(key, value));
  }

  if (f.ReadLine().size()) UTIL_THROW(FormatLoadException, "Expected blank line after " << n << "-grams at byte " << f.Offset());
  store.FinishedInserting();
}

} // namespace
namespace detail {

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

std::size_t Align8(std::size_t in) {
  std::size_t off = in % 8;
  if (!off) return in;
  return in + 8 - off;
}

std::size_t TotalHeaderSize(unsigned int order) {
  return Align8(sizeof(BinaryFileHeader) + 1 /* order */ + sizeof(uint64_t) * order /* counts */ + sizeof(float) /* probing multiplier */ + 1 /* search_tag */);
}

void ReadBinaryHeader(const void *from, off_t size, std::vector<size_t> &out, float &probing_multiplier, unsigned char &search_tag) {
  const char *from_char = reinterpret_cast<const char*>(from);
  if (size < static_cast<off_t>(1 + sizeof(BinaryFileHeader))) UTIL_THROW(FormatLoadException, "File too short to have count information.");
  // Skip over the BinaryFileHeader which was read by IsBinaryFormat.  
  from_char += sizeof(BinaryFileHeader);
  unsigned char order = *reinterpret_cast<const unsigned char*>(from_char);
  if (size < static_cast<off_t>(TotalHeaderSize(order))) UTIL_THROW(FormatLoadException, "File too short to have full header.");
  out.resize(static_cast<std::size_t>(order));
  const uint64_t *counts = reinterpret_cast<const uint64_t*>(from_char + 1);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = static_cast<std::size_t>(counts[i]);
  }
  const float *probing_ptr = reinterpret_cast<const float*>(counts + out.size());
  probing_multiplier = *probing_ptr;
  search_tag = *reinterpret_cast<const char*>(probing_ptr + 1);
}

void WriteBinaryHeader(void *to, const std::vector<size_t> &from, float probing_multiplier, char search_tag) {
  BinaryFileHeader header = BinaryFileHeader();
  header.SetToReference();
  memcpy(to, &header, sizeof(BinaryFileHeader));
  char *out = reinterpret_cast<char*>(to) + sizeof(BinaryFileHeader);
  *reinterpret_cast<unsigned char*>(out) = static_cast<unsigned char>(from.size());
  uint64_t *counts = reinterpret_cast<uint64_t*>(out + 1);
  for (std::size_t i = 0; i < from.size(); ++i) {
    counts[i] = from[i];
  }
  float *probing_ptr = reinterpret_cast<float*>(counts + from.size());
  *probing_ptr = probing_multiplier;
  *reinterpret_cast<char*>(probing_ptr + 1) = search_tag;
}

template <class Search, class VocabularyT> GenericModel<Search, VocabularyT>::GenericModel(const char *file, Config config) : mapped_file_(util::OpenReadOrThrow(file)) {
  const off_t file_size = util::SizeFile(mapped_file_.get());

  std::vector<size_t> counts;

  if (IsBinaryFormat(mapped_file_.get(), file_size)) {
    memory_.reset(util::MapForRead(file_size, config.prefault, mapped_file_.get()), file_size);

    unsigned char search_tag;
    ReadBinaryHeader(memory_.begin(), file_size, counts, config.probing_multiplier, search_tag);
    if (config.probing_multiplier < 1.0) UTIL_THROW(FormatLoadException, "Binary format claims to have a probing multiplier of " << config.probing_multiplier << " which is < 1.0.");
    if (search_tag != Search::kBinaryTag) UTIL_THROW(FormatLoadException, "The binary file has a different search strategy than the one requested.");
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
    if (config.probing_multiplier <= 1.0) UTIL_THROW(FormatLoadException, "probing multiplier must be > 1.0");

    util::FilePiece f(file, mapped_file_.release(), config.messages);
    ReadARPACounts(f, counts);
    size_t memory_size = Size(counts, config);
    char *start;

    if (config.write_mmap) {
      // Write out an mmap file.  
      util::MapZeroedWrite(config.write_mmap, TotalHeaderSize(counts.size()) + memory_size, mapped_file_, memory_);
      WriteBinaryHeader(memory_.get(), counts, config.probing_multiplier, Search::kBinaryTag);
      start = reinterpret_cast<char*>(memory_.get()) + TotalHeaderSize(counts.size());
    } else {
      memory_.reset(util::MapAnonymous(memory_size), memory_size);
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
  bool saw_unk = vocab_.SawUnk();
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

template <class Search, class VocabularyT> FullScoreReturn GenericModel<Search, VocabularyT>::FullScore(const State &in_state, const WordIndex new_word, State &out_state) const {
  unsigned char backoff_start;
  FullScoreReturn ret = ScoreExceptBackoff(in_state.history_, in_state.history_ + in_state.valid_length_, new_word, backoff_start, out_state);
  if (backoff_start - 1 < in_state.valid_length_) {
    ret.prob = std::accumulate(in_state.backoff_ + backoff_start - 1, in_state.backoff_ + in_state.valid_length_, ret.prob);
  }
  return ret;
}

template <class Search, class VocabularyT> FullScoreReturn GenericModel<Search, VocabularyT>::FullScoreForgotState(const WordIndex *context_rbegin, const WordIndex *context_rend, const WordIndex new_word, State &out_state) const {
  unsigned char backoff_start;
  context_rend = std::min(context_rend, context_rbegin + P::Order() - 1);
  FullScoreReturn ret = ScoreExceptBackoff(context_rbegin, context_rend, new_word, backoff_start, out_state);
  ret.prob += SlowBackoffLookup(context_rbegin, context_rend, backoff_start);
  return ret;
}

/* Ugly optimized function.  Produce a score excluding backoff.  
 * The search goes in increasing order of ngram length.  
 * Context goes backward, so context_begin is the word immediately preceeding
 * new_word.  
 */
template <class Search, class VocabularyT> FullScoreReturn GenericModel<Search, VocabularyT>::ScoreExceptBackoff(
    const WordIndex *context_rbegin,
    const WordIndex *context_rend,
    const WordIndex new_word,
    unsigned char &backoff_start,
    State &out_state) const {
  FullScoreReturn ret;
  const ProbBackoff &unigram = unigram_[new_word];
  if (new_word == 0) {
    ret.ngram_length = out_state.valid_length_ = 0;
    // All of backoff.  
    backoff_start = 1;
    ret.prob = unigram.prob;
    return ret;
  }
  float *backoff_out(out_state.backoff_);
  *backoff_out = unigram.backoff;
  ret.prob = unigram.prob;
  out_state.history_[0] = new_word;
  if (context_rbegin == context_rend) {
    ret.ngram_length = out_state.valid_length_ = 1;
    // No backoff because we don't have the history for it.  
    backoff_start = P::Order();
    return ret;
  }
  ++backoff_out;

  // Ok now we now that the bigram contains known words.  Start by looking it up.

  uint64_t lookup_hash = static_cast<uint64_t>(new_word);
  const WordIndex *hist_iter = context_rbegin;
  typename std::vector<Middle>::const_iterator mid_iter = middle_.begin();
  for (; ; ++mid_iter, ++hist_iter, ++backoff_out) {
    if (hist_iter == context_rend) {
      // Ran out of history.  No backoff.  
      backoff_start = P::Order();
      std::copy(context_rbegin, context_rend, out_state.history_ + 1);
      ret.ngram_length = out_state.valid_length_ = (context_rend - context_rbegin) + 1;
      // ret.prob was already set.
      return ret;
    }
    lookup_hash = CombineWordHash(lookup_hash, *hist_iter);
    if (mid_iter == middle_.end()) break;
    typename Middle::ConstIterator found;
    if (!mid_iter->Find(lookup_hash, found)) {
      // Didn't find an ngram using hist_iter.  
      // The history used in the found n-gram is [context_rbegin, hist_iter).  
      std::copy(context_rbegin, hist_iter, out_state.history_ + 1);
      // Therefore, we found a (hist_iter - context_rbegin + 1)-gram including the last word.  
      ret.ngram_length = out_state.valid_length_ = (hist_iter - context_rbegin) + 1;
      backoff_start = mid_iter - middle_.begin() + 1;
      // ret.prob was already set.  
      return ret;
    }
    *backoff_out = found->GetValue().backoff;
    ret.prob = found->GetValue().prob;
  }

  // It passed every lookup in middle_.  That means it's at least a (P::Order() - 1)-gram. 
  // All that's left is to check longest_.  
  
  typename Longest::ConstIterator found;
  if (!longest_.Find(lookup_hash, found)) {
    // It's an (P::Order()-1)-gram
    std::copy(context_rbegin, context_rbegin + P::Order() - 2, out_state.history_ + 1);
    ret.ngram_length = out_state.valid_length_ = P::Order() - 1;
    backoff_start = P::Order() - 1;
    // ret.prob was already set.  
    return ret;
  }
  // It's an P::Order()-gram
  // out_state.valid_length_ is still P::Order() - 1 because the next lookup will only need that much.
  std::copy(context_rbegin, context_rbegin + P::Order() - 2, out_state.history_ + 1);
  out_state.valid_length_ = P::Order() - 1;
  ret.ngram_length = P::Order();
  ret.prob = found->GetValue().prob;
  backoff_start = P::Order();
  return ret;
}

template <class Search, class VocabularyT> float GenericModel<Search, VocabularyT>::SlowBackoffLookup(
    const WordIndex *const context_rbegin, const WordIndex *const context_rend, unsigned char start) const {
  // Add the backoff weights for n-grams of order start to (context_rend - context_rbegin).
  if (context_rend - context_rbegin < static_cast<std::ptrdiff_t>(start)) return 0.0;
  float ret = 0.0;
  if (start == 1) {
    ret += unigram_[*context_rbegin].backoff;
    start = 2;
  }
  uint64_t lookup_hash = static_cast<uint64_t>(*context_rbegin);
  const WordIndex *i;
  for (i = context_rbegin + 1; i < context_rbegin + start - 1; ++i) {
    lookup_hash = CombineWordHash(lookup_hash, *i);
  }
  typename Middle::ConstIterator found;
  // i is the order of the backoff we're looking for.
  for (; i < context_rend; ++i) {
    lookup_hash = CombineWordHash(lookup_hash, *i);
    if (!middle_[i - context_rbegin - 1].Find(lookup_hash, found)) break;
    ret += found->GetValue().backoff;
  }
  return ret;
}

template class GenericModel<ProbingSearch, ProbingVocabulary>;
template class GenericModel<SortedUniformSearch, SortedVocabulary>;

} // namespace detail
} // namespace ngram
} // namespace lm
