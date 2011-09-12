#ifndef LM_LEFT__
#define LM_LEFT__

#include "lm/virtual_interface.hh"
#include "lm/max_order.hh"
#include "lm/model.hh"

namespace lm {
namespace ngram {

struct Left {
  bool operator==(const Left &other) const {
    return 
      (valid_length == other.valid_length) && 
      !memcmp(words, other.words, sizeof(WordIndex) * valid_length);
  }

  int Compare(const Left &other) const {
    if (valid_length != other.valid_length) {
      return (int)valid_length - (int)other.valid_length;
    }
    return memcmp(words, other.words, sizeof(WordIndex) * valid_length);
  }

  WordIndex words[kMaxOrder - 1];
  unsigned char valid_length;
};

struct ChartState {
  bool operator==(const ChartState &other) {
    return (left == other.left) && (right == other.right) && (charge_backoff == other.charge_backoff);
  }

  int Compare(const ChartState &other) const {
    int lres = left.Compare(other.left);
    if (lres) return lres;
    int rres = right.Compare(other.right);
    if (rres) return rres;
    return (int)charge_backoff - (int)other.charge_backoff;
  }

  Left left;
  State right;
  bool charge_backoff;
  float left_est;
};

template <class M> class RuleScore {
  public:
    explicit RuleScore(const M &model, ChartState &out) : model_(model), out_(out), left_done_(false), left_write_(out.left.words), prob_(0.0) {
      out.left.valid_length = 0;
      out.right.valid_length_ = 0;
      out.charge_backoff = false;
      out.left_est = 0.0;
    }

    void BeginSentence() {
      out_.right = model_.BeginSentenceState();
      // out_.left is empty.
      left_done_ = true;
    }

    void Terminal(WordIndex word) {
      State copy(out_.right);
      FullScoreReturn ret(model_.FullScore(copy, word, out_.right));
      prob_ += ret.prob;
      if (left_done_) return;

      if (ret.independent_left /* left_write_ == out_.left.words + model_.Order() - 1*/) {
        out_.charge_backoff = true;
        left_done_ = true;
        return;
      }
      *(left_write_++) = word;
      out_.left_est += ret.prob;
    }

    void NonTerminal(const ChartState &in, float prob) {
      prob_ += prob - in.left_est;
      for (const WordIndex *i = in.left.words; i != in.left.words + in.left.valid_length; ++i) {
        Terminal(*i);
      }
      if (in.charge_backoff) {
        // The last word of the left state was eliminated for recombination purposes.  
        // Charge backoff for n-grams that start before left state.  
        float charges = 0.0;
        for (const float *i = out_.right.backoff_ + in.left.valid_length; i < out_.right.backoff_ + out_.right.valid_length_; ++i)
          charges += *i;
        prob_ += charges;
        if (!left_done_) {
          out_.charge_backoff = true;
          left_done_ = true;
        }
        out_.right = in.right;
      } else {
        if (in.left.valid_length == model_.Order() - 1) out_.right = in.right;
      }
    }

    float Finish() {
      out_.left.valid_length = left_write_ - out_.left.words;
      return prob_;
    }

  private:
    const M &model_;

    ChartState &out_;

    bool left_done_;

    WordIndex *left_write_;

    float prob_;
};

} // namespace ngram
} // namespace lm

#endif // LM_LEFT__
