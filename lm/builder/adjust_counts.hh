#ifndef LM_BUILDER_ADJUST_COUNTS_H
#define LM_BUILDER_ADJUST_COUNTS_H

#include "lm/builder/discount.hh"
#include "util/exception.hh"

#include <vector>

#include <stdint.h>

namespace util { namespace stream { class ChainPositions; } }

namespace lm {
namespace builder {

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
    AdjustCounts(std::vector<uint64_t> &counts, std::vector<uint64_t> &counts_pruned, std::vector<Discount> &discounts, std::vector<uint64_t> &prune_thresholds)
      : counts_(counts), counts_pruned_(counts_pruned), discounts_(discounts), prune_thresholds_(prune_thresholds)
    {}

    void Run(const util::stream::ChainPositions &positions);

  private:
    std::vector<uint64_t> &counts_;
    std::vector<uint64_t> &counts_pruned_;
    std::vector<Discount> &discounts_;
    std::vector<uint64_t> &prune_thresholds_; 
};

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_ADJUST_COUNTS_H

