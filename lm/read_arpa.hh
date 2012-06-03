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
void ReadNGramHeader(util::FilePiece &in, unsigned int length);

void ReadBackoff(util::FilePiece &in, Prob &weights);
void ReadBackoff(util::FilePiece &in, float &backoff);
inline void ReadBackoff(util::FilePiece &in, ProbBackoff &weights) {
  ReadBackoff(in, weights.backoff);
}
inline void ReadBackoff(util::FilePiece &in, RestWeights &weights) {
  ReadBackoff(in, weights.backoff);
}

void ReadEnd(util::FilePiece &in);

extern const bool kARPASpaces[256];

// Positive log probability warning.  
class PositiveProbWarn {
  public:
    PositiveProbWarn() : action_(THROW_UP) {}

    explicit PositiveProbWarn(WarningAction action) : action_(action) {}

    void Warn(float prob);

  private:
    WarningAction action_;
};

template <class Voc, class Weights> void Read1Gram(util::FilePiece &f, Voc &vocab, Weights *unigrams, PositiveProbWarn &warn) {
  try {
    float prob = f.ReadFloat();
    if (prob > 0.0) {
      warn.Warn(prob);
      prob = 0.0;
    }
    if (f.get() != '\t') UTIL_THROW(FormatLoadException, "Expected tab after probability");
    Weights &value = unigrams[vocab.Insert(f.ReadDelimited(kARPASpaces))];
    value.prob = prob;
    ReadBackoff(f, value);
  } catch(util::Exception &e) {
    e << " in the 1-gram at byte " << f.Offset();
    throw;
  }
}

// Return true if a positive log probability came out.
template <class Voc, class Weights> void Read1Grams(util::FilePiece &f, std::size_t count, Voc &vocab, Weights *unigrams, PositiveProbWarn &warn) {
  ReadNGramHeader(f, 1);
  for (std::size_t i = 0; i < count; ++i) {
    Read1Gram(f, vocab, unigrams, warn);
  }
  vocab.FinishedLoading(unigrams);
}

// Return true if a positive log probability came out.
template <class Voc, class Weights> void ReadNGram(util::FilePiece &f, const unsigned char n, const Voc &vocab, WordIndex *const reverse_indices, Weights &weights, PositiveProbWarn &warn) {
  try {
    weights.prob = f.ReadFloat();
    if (weights.prob > 0.0) {
      warn.Warn(weights.prob);
      weights.prob = 0.0;
    }
    for (WordIndex *vocab_out = reverse_indices + n - 1; vocab_out >= reverse_indices; --vocab_out) {
      *vocab_out = vocab.Index(f.ReadDelimited(kARPASpaces));
    }
    ReadBackoff(f, weights);
  } catch(util::Exception &e) {
    e << " in the " << static_cast<unsigned int>(n) << "-gram at byte " << f.Offset();
    throw;
  }
}

} // namespace lm

#endif // LM_READ_ARPA__
