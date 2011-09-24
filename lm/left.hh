#ifndef LM_LEFT__
#define LM_LEFT__

#include "lm/max_order.hh"
#include "lm/model.hh"
#include "lm/return.hh"

#include <algorithm>

namespace lm {
namespace ngram {

struct Left {
  bool operator==(const Left &other) const {
    return 
      (length == other.length) && 
      pointers[length - 1] == other.pointers[length - 1];
  }

  int Compare(const Left &other) const {
    if (length != other.length) {
      return (int)length - (int)other.length;
    }
    if (pointers[length - 1] > other.pointers[length - 1]) return 1;
    if (pointers[length - 1] < other.pointers[length - 1]) return -1;
    return 0;
  }

  void ZeroRemaining() {
    for (uint64_t * i = pointers + length; i < pointers + kMaxOrder - 1; ++i)
      *i = 0;
  }

  uint64_t pointers[kMaxOrder - 1];
  unsigned char length;
};

struct ChartState {
  bool operator==(const ChartState &other) {
    return (left == other.left) && (right == other.right) && (full == other.full);
  }

  int Compare(const ChartState &other) const {
    int lres = left.Compare(other.left);
    if (lres) return lres;
    int rres = right.Compare(other.right);
    if (rres) return rres;
    return (int)full - (int)other.full;
  }

  void ZeroRemaining() {
    left.ZeroRemaining();
    right.ZeroRemaining();
  }

  Left left;
  State right;
  bool full;
};

template <class M> class RuleScore {
  public:
    explicit RuleScore(const M &model, ChartState &out) : model_(model), out_(out), left_done_(false), left_write_(out.left.pointers), prob_(0.0) {
      out.left.length = 0;
      out.right.length = 0;
    }

    void BeginSentence() {
      out_.right = model_.BeginSentenceState();
      // out_.left is empty.
      left_done_ = true;
    }

    void Terminal(WordIndex word) {
      State copy(out_.right);
      FullScoreReturn ret = model_.FullScore(copy, word, out_.right);
      ProcessRet(ret);
      if (out_.right.length != copy.length + 1) left_done_ = true;
    }

    // Faster version of NonTerminal for the case where the rule begins with a non-terminal.  
    void BeginNonTerminal(const ChartState &in, float prob) {
      prob_ = prob;
      out_ = in;
      left_write_ = out_.left.pointers + out_.left.length;
      left_done_ = in.full;
    }

    void NonTerminal(const ChartState &in, float prob) {
      prob_ += prob;
      
      if (!in.left.length) {
        if (in.full) {
          for (const float *i = out_.right.backoff; i < out_.right.backoff + out_.right.length; ++i) prob_ += *i;
          left_done_ = true;
          out_.right = in.right;
        }
        return;
      }

      if (!out_.right.length) {
        out_.right = in.right;
        if (left_done_) return;
        if (left_write_ != out_.left.pointers) {
          left_done_ = true;
        } else {
          out_.left = in.left;
          left_write_ = out_.left.pointers + in.left.length;
          left_done_ = in.full;
        }
        return;
      }

      float backoffs[kMaxOrder - 1], backoffs2[kMaxOrder - 1];
      float *back = backoffs, *back2 = backoffs2;
      unsigned char next_use;
      FullScoreReturn ret;
      ProcessRet(ret = model_.ExtendLeft(out_.right.words, out_.right.words + out_.right.length, out_.right.backoff, in.left.pointers[0], 1, back, next_use));
      if (!next_use) {
        left_done_ = true;
        out_.right = in.right;
        return;
      }
      unsigned char extend_length = 2;
      for (const uint64_t *i = in.left.pointers + 1; i < in.left.pointers + in.left.length; ++i, ++extend_length) {
        ProcessRet(ret = model_.ExtendLeft(out_.right.words, out_.right.words + next_use, back, *i, extend_length, back2, next_use));
        if (!next_use) {
          left_done_ = true;
          out_.right = in.right;
          return;
        }
        std::swap(back, back2);
      }

      if (in.full) {
        for (const float *i = back; i != back + next_use; ++i) prob_ += *i;
        left_done_ = true;
        out_.right = in.right;
        return;
      }

      // Right state was minimized, so it's already independent of the new words to the left.  
      if (in.right.length < in.left.length) {
        out_.right = in.right;
        return;
      }

      // Shift exisiting words down.  
      for (WordIndex *i = out_.right.words + next_use - 1; i >= out_.right.words; --i) {
        *(i + in.right.length) = *i;
      }
      // Add words from in.right.  
      std::copy(in.right.words, in.right.words + in.right.length, out_.right.words);
      // Assemble backoff composed on the existing state's backoff followed by the new state's backoff.  
      std::copy(in.right.backoff, in.right.backoff + in.right.length, out_.right.backoff);
      std::copy(back, back + next_use, out_.right.backoff + in.right.length);
      out_.right.length = in.right.length + next_use;
    }

    float Finish() {
      out_.left.length = left_write_ - out_.left.pointers;
      out_.full = left_done_;
      return prob_;
    }

  private:
    void ProcessRet(const FullScoreReturn &ret) {
      prob_ += ret.prob;
      if (left_done_) return;
      if (ret.independent_left) {
        left_done_ = true;
        return;
      }
      *(left_write_++) = ret.extend_left;
    }

    const M &model_;

    ChartState &out_;

    bool left_done_;

    uint64_t *left_write_;

    float prob_;
};

} // namespace ngram
} // namespace lm

#endif // LM_LEFT__
