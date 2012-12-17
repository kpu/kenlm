#include "lm/builder/interpolate.hh"

#include "lm/builder/joint_order.hh"
#include "lm/builder/multi_stream.hh"
#include "lm/lm_exception.hh"

#include <assert.h>

namespace lm { namespace builder {
namespace {

class Callback {
  public:
    Callback(std::size_t order, uint64_t unigram_count) : probs_(order + 1) {
      probs_[0] = 1.0 / static_cast<float>(unigram_count - 1); // exclude <s> from unigram count
    }

    void Enter(unsigned order_minus_1, NGram &gram) {
      // Ordering is important so as to not overwrite gamma.  
      Payload &pay = gram.Value();
      pay.interp.prob = pay.uninterp.prob + pay.uninterp.gamma * probs_[order_minus_1];
      pay.interp.lower = probs_[order_minus_1];
      probs_[order_minus_1 + 1] = pay.interp.prob;
    }

    void Exit(unsigned, const NGram &) const {}

  private:
    std::vector<float> probs_;
};
} // namespace

void Interpolate::Run(const ChainPositions &positions) {
  Callback callback(positions.size(), unigram_count_);
  JointOrder<Callback>(positions, callback);
}

}} // namespaces
