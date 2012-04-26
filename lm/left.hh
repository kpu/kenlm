/* Efficient left and right language model state for sentence fragments.
 * Intended usage:
 * Store ChartState with every chart entry.  
 * To do a rule application:
 * 1. Make a ChartState object for your new entry.  
 * 2. Construct RuleScore.  
 * 3. Going from left to right, call Terminal or NonTerminal. 
 *   For terminals, just pass the vocab id.  
 *   For non-terminals, pass that non-terminal's ChartState.
 *     If your decoder expects scores inclusive of subtree scores (i.e. you
 *     label entries with the highest-scoring path), pass the non-terminal's
 *     score as prob.  
 *     If your decoder expects relative scores and will walk the chart later,
 *     pass prob = 0.0.  
 *     In other words, the only effect of prob is that it gets added to the
 *     returned log probability.  
 * 4. Call Finish.  It returns the log probability.   
 *
 * There's a couple more details: 
 * Do not pass <s> to Terminal as it is formally not a word in the sentence,
 * only context.  Instead, call BeginSentence.  If called, it should be the
 * first call after RuleScore is constructed (since <s> is always the
 * leftmost).
 *
 * If the leftmost RHS is a non-terminal, it's faster to call BeginNonTerminal.
 *
 * Hashing and sorting comparison operators are provided.   All state objects
 * are POD.  If you intend to use memcmp on raw state objects, you must call
 * ZeroRemaining first, as the value of array entries beyond length is
 * otherwise undefined.  
 *
 * Usage is of course not limited to chart decoding.  Anything that generates
 * sentence fragments missing left context could benefit.  For example, a
 * phrase-based decoder could pre-score phrases, storing ChartState with each
 * phrase, even if hypotheses are generated left-to-right.  
 */

#ifndef LM_LEFT__
#define LM_LEFT__

#include "lm/max_order.hh"
#include "lm/model.hh"
#include "lm/return.hh"

#include "util/murmur_hash.hh"

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
    if (length != other.length) return length < other.length ? -1 : 1;
    if (pointers[length - 1] > other.pointers[length - 1]) return 1;
    if (pointers[length - 1] < other.pointers[length - 1]) return -1;
    return 0;
  }

  bool operator<(const Left &other) const {
    if (length != other.length) return length < other.length;
    return pointers[length - 1] < other.pointers[length - 1];
  }

  void ZeroRemaining() {
    for (uint64_t * i = pointers + length; i < pointers + kMaxOrder - 1; ++i)
      *i = 0;
  }

  unsigned char length;
  uint64_t pointers[kMaxOrder - 1];
};

inline size_t hash_value(const Left &left) {
  return util::MurmurHashNative(&left.length, 1, left.length ? left.pointers[left.length - 1] : 0);
}

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

  bool operator<(const ChartState &other) const {
    return Compare(other) == -1;
  }

  void ZeroRemaining() {
    left.ZeroRemaining();
    right.ZeroRemaining();
  }

  Left left;
  bool full;
  State right;
};

inline size_t hash_value(const ChartState &state) {
  size_t hashes[2];
  hashes[0] = hash_value(state.left);
  hashes[1] = hash_value(state.right);
  return util::MurmurHashNative(hashes, sizeof(size_t) * 2, state.full);
}

template <class M> class RuleScore {
  public:
    explicit RuleScore(const M &model, ChartState &out) : model_(model), out_(out), left_done_(false), prob_(0.0) {
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
      FullScoreReturn ret(model_.FullScore(copy, word, out_.right));
      prob_ += ret.prob;
      if (left_done_) return;
      if (ret.independent_left) {
        left_done_ = true;
        return;
      }
      out_.left.pointers[out_.left.length++] = ret.extend_left;
      if (out_.right.length != copy.length + 1)
        left_done_ = true;
    }

    // Faster version of NonTerminal for the case where the rule begins with a non-terminal.  
    void BeginNonTerminal(const ChartState &in, float prob) {
      prob_ = prob;
      out_ = in;
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
        if (out_.left.length) {
          left_done_ = true;
        } else {
          out_.left = in.left;
          left_done_ = in.full;
        }
        return;
      }

      float backoffs[kMaxOrder - 1], backoffs2[kMaxOrder - 1];
      float *back = backoffs, *back2 = backoffs2;
      unsigned char next_use = out_.right.length;

      // First word
      if (ExtendLeft(in, next_use, 1, out_.right.backoff, back)) return;

      // Words after the first, so extending a bigram to begin with
      for (unsigned char extend_length = 2; extend_length <= in.left.length; ++extend_length) {
        if (ExtendLeft(in, next_use, extend_length, back, back2)) return;
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
      // A N-1-gram might extend left and right but we should still set full to true because it's an N-1-gram.  
      out_.full = left_done_ || (out_.left.length == model_.Order() - 1);
      return prob_;
    }

  private:
    bool ExtendLeft(const ChartState &in, unsigned char &next_use, unsigned char extend_length, const float *back_in, float *back_out) {
      ProcessRet(model_.ExtendLeft(
            out_.right.words, out_.right.words + next_use, // Words to extend into
            back_in, // Backoffs to use
            in.left.pointers[extend_length - 1], extend_length, // Words to be extended
            back_out, // Backoffs for the next score
            next_use)); // Length of n-gram to use in next scoring.  
      if (next_use != out_.right.length) {
        left_done_ = true;
        if (!next_use) {
          out_.right = in.right;
          // Early exit.  
          return true;
        }
      }
      // Continue scoring.  
      return false;
    }

    void ProcessRet(const FullScoreReturn &ret) {
      prob_ += ret.prob;
      if (left_done_) return;
      if (ret.independent_left) {
        left_done_ = true;
        return;
      }
      out_.left.pointers[out_.left.length++] = ret.extend_left;
    }

    const M &model_;

    ChartState &out_;

    bool left_done_;

    float prob_;
};

} // namespace ngram
} // namespace lm

#endif // LM_LEFT__
