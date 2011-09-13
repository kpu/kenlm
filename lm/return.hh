#ifndef LM_RETURN__
#define LM_RETURN__

#include <inttypes.h>

namespace lm {
/* Structure returned by scoring routines. */
struct FullScoreReturn {
  // log10 probability
  float prob;

  /* The length of n-gram matched.  Do not use this for recombination.  
   * Consider a model containing only the following n-grams:
   * -1 foo
   * -3.14  bar
   * -2.718 baz -5
   * -6 foo bar
   *
   * If you score ``bar'' then ngram_length is 1 and recombination state is the
   * empty string because bar has zero backoff and does not extend to the
   * right.  
   * If you score ``foo'' then ngram_length is 1 and recombination state is 
   * ``foo''.  
   *
   * Ideally, keep output states around and compare them.  Failing that,
   * get out_state.ValidLength() and use that length for recombination.
   */
  unsigned char ngram_length;

  /* Left extension information.  If the n-gram's probability is independent
   * (up to additional backoff) of words to the left, extend_left is set to
   * kIndependentLeft.  Otherwise, it indicates an efficient way to extend
   * left.  
   */
  static const uint64_t kIndependentLeft = (uint64_t)-1;

  bool IndependentLeft() const {
    return extend_left == kIndependentLeft;
  }

  uint64_t extend_left;
};

} // namespace lm
#endif // LM_RETURN__
