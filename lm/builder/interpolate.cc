#include "lm/builder/interpolate.hh"

#include "lm/builder/joint_order.hh"
#include "lm/builder/multi_stream.hh"
#include "lm/builder/sort.hh"
#include "lm/lm_exception.hh"

#include <assert.h>

namespace lm { namespace builder {
namespace {

class Callback {
  public:
    Callback(std::size_t order, float uniform_prob) : probs_(order + 1) {
      probs_[0] = uniform_prob;
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

Interpolate::Interpolate(uint64_t unigram_count) 
  : uniform_prob_(1.0 / static_cast<float>(unigram_count - 1)) {}

void Interpolate::Run(const ChainPositions &positions) {
  Callback callback(positions.size(), uniform_prob_);
  JointOrder<Callback, SuffixOrder>(positions, callback);
}

}} // namespaces
