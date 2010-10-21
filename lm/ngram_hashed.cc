#include "lm/ngram_hashed.hh"

#include "lm/exception.hh"
#include "lm/read_arpa.hh"
#include "lm/vocab.hh"

#include "util/file_piece.hh"

#include <string>

namespace lm {
namespace ngram {

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

template <class MiddleT, class LongestT> template <class Voc> void TemplateHashedSearch<MiddleT, LongestT>::InitializeFromARPA(const char * /*file*/, util::FilePiece &f, const std::vector<uint64_t> &counts, const Config &/*config*/, Voc &vocab) {
  Read1Grams(f, counts[0], vocab, unigram.Raw());  
  // Read the n-grams.
  for (unsigned int n = 2; n < counts.size(); ++n) {
    ReadNGrams(f, n, counts[n-1], vocab, middle[n-2]);
  }
  ReadNGrams(f, counts.size(), counts[counts.size() - 1], vocab, longest);
}

template void TemplateHashedSearch<ProbingHashedSearch::Middle, ProbingHashedSearch::Longest>::InitializeFromARPA(const char *, util::FilePiece &f, const std::vector<uint64_t> &counts, const Config &, ProbingVocabulary &vocab);
template void TemplateHashedSearch<SortedHashedSearch::Middle, SortedHashedSearch::Longest>::InitializeFromARPA(const char *, util::FilePiece &f, const std::vector<uint64_t> &counts, const Config &, SortedVocabulary &vocab);

} // namespace detail
} // namespace ngram
} // namespace lm
