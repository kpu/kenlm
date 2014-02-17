#ifndef LM_BUILDER_INTERPOLATE__
#define LM_BUILDER_INTERPOLATE__

#include "lm/builder/multi_stream.hh"
#include "lm/word_index.hh"

#include <stdint.h>

namespace lm { namespace builder {

class Binarize;
 
/* Interpolate step.  
 * Input: suffix sorted n-grams with (p_uninterpolated, gamma) from
 * InitialProbabilities.
 * Output: suffix sorted n-grams with complete probability
 */
class Interpolate {
  public:
    // Normally the unigram count-1 (since p(<s>) = 0) but might be larger to
    // set a consistent vocabulary size.
    explicit Interpolate(uint64_t vocab_size, Binarize &binarize, const ChainPositions &backoffs);

    void Run(const ChainPositions &positions);

  private:
    float uniform_prob_;
    Binarize &binarize_;
    ChainPositions backoffs_;
};

}} // namespaces
#endif // LM_BUILDER_INTERPOLATE__
