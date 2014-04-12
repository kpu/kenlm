#ifndef LM_READ_ARPA_H
#define LM_READ_ARPA_H

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

template <class Weights> StringPiece Read1Gram(util::FilePiece &f, Weights &weights, PositiveProbWarn &warn) {
  try {
    weights.prob = f.ReadFloat();
    if (weights.prob > 0.0) {
      warn.Warn(weights.prob);
      weights.prob = 0.0;
    }
    UTIL_THROW_IF(f.get() != '\t', FormatLoadException, "Expected tab after probability");
    StringPiece ret(f.ReadDelimited(kARPASpaces));
    ReadBackoff(f, weights);
    return ret;
  } catch(util::Exception &e) {
    e << " in the 1-gram at byte " << f.Offset();
    throw;
  }
}

template <class Voc, class Weights> void Read1Gram(util::FilePiece &f, Voc &vocab, Weights *unigrams, PositiveProbWarn &warn) {
  Weights temp;
  WordIndex word = vocab.Insert(Read1Gram(f, temp, warn));
  unigrams[word] = temp;
}

template <class Voc, class Weights> void Read1Grams(util::FilePiece &f, std::size_t count, Voc &vocab, Weights *unigrams, PositiveProbWarn &warn) {
  ReadNGramHeader(f, 1);
  for (std::size_t i = 0; i < count; ++i) {
    Read1Gram(f, vocab, unigrams, warn);
  }
  vocab.FinishedLoading(unigrams);
}

// Read ngram, write vocab ids to indices_out.
template <class Voc, class Weights, class Iterator> void ReadNGram(util::FilePiece &f, const unsigned char n, const Voc &vocab, Iterator indices_out, Weights &weights, PositiveProbWarn &warn) {
  try {
    weights.prob = f.ReadFloat();
    if (weights.prob > 0.0) {
      warn.Warn(weights.prob);
      weights.prob = 0.0;
    }
    for (unsigned char i = 0; i < n; ++i, ++indices_out) {
      *indices_out = vocab.Index(f.ReadDelimited(kARPASpaces));
    }
    ReadBackoff(f, weights);
  } catch(util::Exception &e) {
    e << " in the " << static_cast<unsigned int>(n) << "-gram at byte " << f.Offset();
    throw;
  }
}

} // namespace lm

#endif // LM_READ_ARPA_H
