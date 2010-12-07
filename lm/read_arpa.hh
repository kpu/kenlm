#ifndef LM_READ_ARPA__
#define LM_READ_ARPA__

#include "lm/lm_exception.hh"
#include "lm/word_index.hh"
#include "lm/weights.hh"
#include "util/file_piece.hh"

#include <cstddef>
#include <iosfwd>
#include <vector>

namespace lm {

void ReadARPACounts(util::FilePiece &in, std::vector<uint64_t> &number);
void ReadARPACounts(std::istream &in, std::vector<uint64_t> &number);
void ReadNGramHeader(util::FilePiece &in, unsigned int length);
void ReadNGramHeader(std::istream &in, unsigned int length);

void ReadBackoff(util::FilePiece &in, Prob &weights);
void ReadBackoff(util::FilePiece &in, ProbBackoff &weights);

void ReadEnd(util::FilePiece &in);
void ReadEnd(std::istream &in);

template <class Voc> void Read1Gram(util::FilePiece &f, Voc &vocab, ProbBackoff *unigrams) {
  try {
    float prob = f.ReadFloat();
    if (prob > 0) UTIL_THROW(FormatLoadException, "Positive probability " << prob);
    if (f.get() != '\t') UTIL_THROW(FormatLoadException, "Expected tab after probability");
    ProbBackoff &value = unigrams[vocab.Insert(f.ReadDelimited())];
    value.prob = prob;
    ReadBackoff(f, value);
  } catch(util::Exception &e) {
    e << " in the 1-gram at byte " << f.Offset();
    throw;
  }
}

template <class Voc> void Read1Grams(util::FilePiece &f, std::size_t count, Voc &vocab, ProbBackoff *unigrams) {
  ReadNGramHeader(f, 1);
  for (std::size_t i = 0; i < count; ++i) {
    Read1Gram(f, vocab, unigrams);
  }
  vocab.FinishedLoading(unigrams);
}

template <class Voc, class Weights> void ReadNGram(util::FilePiece &f, const unsigned char n, const Voc &vocab, WordIndex *const reverse_indices, Weights &weights) {
  try {
    weights.prob = f.ReadFloat();
    if (weights.prob > 0) UTIL_THROW(FormatLoadException, "Positive probability " << weights.prob);
    for (WordIndex *vocab_out = reverse_indices + n - 1; vocab_out >= reverse_indices; --vocab_out) {
      *vocab_out = vocab.Index(f.ReadDelimited());
    }
    ReadBackoff(f, weights);
  } catch(util::Exception &e) {
    e << " in the " << static_cast<unsigned int>(n) << "-gram at byte " << f.Offset();
    throw;
  }
}

} // namespace lm

#endif // LM_READ_ARPA__
