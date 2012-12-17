#ifndef LM_BUILDER_BACKOFF__
#define LM_BUILDER_BACKOFF__
namespace lm { namespace builder {

class ChainPositions;

/* Compute backoff weights.  
 * Input: context-sorted n-grams with interpolated probabilities.
 * Output: n-grams with probability and backoff.  Second field of N-grams is
 * undefined.  
 */
class Backoff {
  public:
    Backoff() {}

    void Run(const ChainPositions &positions);
};

}} // namespaces
#endif // LM_BUILDER_BACKOFF__
