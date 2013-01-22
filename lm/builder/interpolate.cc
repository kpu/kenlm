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
    Callback(float uniform_prob, const ChainPositions &backoffs) : backoffs_(backoffs.size()), probs_(backoffs.size() + 2) {
      probs_[0] = uniform_prob;
      for (std::size_t i = 0; i < backoffs.size(); ++i) {
        backoffs_.push_back(backoffs[i]);
      }
    }

    ~Callback() {
      for (std::size_t i = 0; i < backoffs_.size(); ++i) {
        if (backoffs_[i]) {
          std::cerr << "Backoffs do not match for order " << (i + 1) << std::endl;
          abort();
        }
      }
    }

    void Enter(unsigned order_minus_1, NGram &gram) {
      Payload &pay = gram.Value();
      pay.complete.prob = pay.uninterp.prob + pay.uninterp.gamma * probs_[order_minus_1];
      probs_[order_minus_1 + 1] = pay.complete.prob;
      pay.complete.prob = log10(pay.complete.prob);
      // TODO: this is a hack to skip n-grams that don't appear as context.  Pruning will require some different handling.  
      if (order_minus_1 < backoffs_.size() && *(gram.end() - 1) != kUNK && *(gram.end() - 1) != kEOS) {
        pay.complete.backoff = log10(*static_cast<const float*>(backoffs_[order_minus_1].Get()));
        ++backoffs_[order_minus_1];
      } else {
        // Not a context.  
        pay.complete.backoff = 0.0;
      }
    }

    void Exit(unsigned, const NGram &) const {}

  private:
    FixedArray<util::stream::Stream> backoffs_;

    std::vector<float> probs_;
};
} // namespace

Interpolate::Interpolate(uint64_t unigram_count, const ChainPositions &backoffs) 
  : uniform_prob_(1.0 / static_cast<float>(unigram_count - 1)), backoffs_(backoffs) {}

// perform order-wise interpolation
void Interpolate::Run(const ChainPositions &positions) {
  assert(positions.size() == backoffs_.size() + 1);
  Callback callback(uniform_prob_, backoffs_);
  JointOrder<Callback, SuffixOrder>(positions, callback);
}

}} // namespaces
