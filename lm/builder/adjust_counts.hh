#ifndef LM_BUILDER_ADJUST_COUNTS__
#define LM_BUILDER_ADJUST_COUNTS__

#include "lm/builder/discount.hh"

#include <vector>

#include <stdint.h>

namespace lm {
namespace builder {

class ChainPositions;

/* Compute adjusted counts.  
 * Input: unique suffix sorted N-grams (and just the N-grams) with raw counts.
 * Output: suffix sorted [1,N]-grams with adjusted counts.  
 * The N-gram output replaces <s> <s> * entries with tombstones consisting of
 * kTombstone and count 0.  These will go to the end on the next sort pass and
 * should be stripped off.  
 */
class AdjustCounts {
  public:
    AdjustCounts(std::vector<uint64_t> &counts, std::vector<Discount> &discounts)
      : counts_(counts), discounts_(discounts) {}

    void Run(const ChainPositions &positions);

  private:
    std::vector<uint64_t> &counts_;
    std::vector<Discount> &discounts_;
};

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_ADJUST_COUNTS__

