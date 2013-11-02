#ifndef LM_BUILDER_ADJUST_COUNTS__
#define LM_BUILDER_ADJUST_COUNTS__

#include "lm/builder/discount.hh"
#include "util/exception.hh"

#include <vector>

#include <stdint.h>

namespace lm {
namespace builder {

class ChainPositions;

class BadDiscountException : public util::Exception {
  public:
    BadDiscountException() throw();
    ~BadDiscountException() throw();
};

/* Compute adjusted counts.  
 * Input: unique suffix sorted N-grams (and just the N-grams) with raw counts.
 * Output: [1,N]-grams with adjusted counts.  
 * [1,N)-grams are in suffix order
 * N-grams are in undefined order (they're going to be sorted anyway).
 */
class AdjustCounts {
  public:
    AdjustCounts(std::vector<uint64_t> &counts, std::vector<Discount> &discounts,
                 std::vector<uint64_t> &prune_thresholds)
      : counts_(counts), discounts_(discounts),
        prune_thresholds_(prune_thresholds) //mjd
    {}

    void Run(const ChainPositions &positions);

  private:
    std::vector<uint64_t> &counts_;
    std::vector<Discount> &discounts_;
    std::vector<uint64_t> &prune_thresholds_; //mjd
};

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_ADJUST_COUNTS__

