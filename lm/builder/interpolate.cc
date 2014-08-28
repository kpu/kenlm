#include "lm/builder/interpolate.hh"

#include "lm/builder/hash_gamma.hh"
#include "lm/builder/joint_order.hh"
#include "lm/builder/ngram_stream.hh"
#include "lm/builder/sort.hh"
#include "lm/lm_exception.hh"
#include "util/fixed_array.hh"
#include "util/murmur_hash.hh"

#include <assert.h>

namespace lm { namespace builder {
namespace {

class Callback {
  public:
    Callback(float uniform_prob, const util::stream::ChainPositions &backoffs, const std::vector<uint64_t> &prune_thresholds)
      : backoffs_(backoffs.size()), probs_(backoffs.size() + 2), prune_thresholds_(prune_thresholds) {
      probs_[0] = uniform_prob;
      for (std::size_t i = 0; i < backoffs.size(); ++i) {
        backoffs_.push_back(backoffs[i]);
      }
    }

    ~Callback() {
      for (std::size_t i = 0; i < backoffs_.size(); ++i) {
        if(prune_thresholds_[i + 1] > 0)
          while(backoffs_[i])
            ++backoffs_[i];
        
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
      
      if (order_minus_1 < backoffs_.size() && *(gram.end() - 1) != kUNK && *(gram.end() - 1) != kEOS) {
        // This skips over ngrams if backoffs have been exhausted.
        if(!backoffs_[order_minus_1]) {
          pay.complete.backoff = 0.0;
          return;
        }

        if(prune_thresholds_[order_minus_1 + 1] > 0) {
          //Compute hash value for current context
          uint64_t current_hash = util::MurmurHashNative(gram.begin(), gram.Order() * sizeof(WordIndex));
          
          const HashGamma *hashed_backoff = static_cast<const HashGamma*>(backoffs_[order_minus_1].Get());
          while(current_hash != hashed_backoff->hash_value && ++backoffs_[order_minus_1])
            hashed_backoff = static_cast<const HashGamma*>(backoffs_[order_minus_1].Get());

          if(current_hash == hashed_backoff->hash_value) {
            pay.complete.backoff = log10(hashed_backoff->gamma);
            ++backoffs_[order_minus_1];
          } else {
            // Has been pruned away so it is not a context anymore
            pay.complete.backoff = 0.0;
          }
        } else {
          pay.complete.backoff = log10(*static_cast<const float*>(backoffs_[order_minus_1].Get()));
          ++backoffs_[order_minus_1];
        }
      } else {
        // Not a context.
        pay.complete.backoff = 0.0;
      }
    }

    void Exit(unsigned, const NGram &) const {}

  private:
    util::FixedArray<util::stream::Stream> backoffs_;

    std::vector<float> probs_;
    const std::vector<uint64_t>& prune_thresholds_;
};
} // namespace

Interpolate::Interpolate(uint64_t vocab_size, const util::stream::ChainPositions &backoffs, const std::vector<uint64_t>& prune_thresholds)
  : uniform_prob_(1.0 / static_cast<float>(vocab_size)), // Includes <unk> but excludes <s>.
    backoffs_(backoffs),
    prune_thresholds_(prune_thresholds) {}

// perform order-wise interpolation
void Interpolate::Run(const util::stream::ChainPositions &positions) {
  assert(positions.size() == backoffs_.size() + 1);
  Callback callback(uniform_prob_, backoffs_, prune_thresholds_);
  JointOrder<Callback, SuffixOrder>(positions, callback);
}

}} // namespaces
