#include "lm/builder/backoff.hh"

#include "lm/builder/joint_order.hh"
#include "lm/builder/multi_stream.hh"
#include "lm/builder/sort.hh"

#include "util/stream/timer.hh"

#include <math.h>

namespace lm { namespace builder {
namespace {

// TODO: finalize <s>, do something with zeroton backoff.  
class Callback {
  public:
    explicit Callback(std::size_t order) {
      Accum entry;
      entry.prob = 0.0;
      entry.lower = 0.0;
      sums_.resize(order + 1, entry);
    }

    void Enter(unsigned order_minus_1, NGram &gram) {
      sums_[order_minus_1].prob += gram.Value().interp.prob;
      sums_[order_minus_1].lower += gram.Value().interp.lower;
    }

    void Exit(unsigned order_minus_1, NGram &gram) {
      // TODO: rounding corner cases.  
      ProbBackoff &out = gram.Value().complete;
      out.prob = std::log10(out.prob);
      out.backoff = log1p(-sums_[order_minus_1 + 1].prob) - log1p(-sums_[order_minus_1 + 1].lower);
      out.backoff /= M_LN10;
      sums_[order_minus_1 + 1].prob = 0.0;
      sums_[order_minus_1 + 1].lower = 0.0;
    }

  private:
    struct Accum {
      double prob;
      double lower;
    };
    std::vector<Accum> sums_;
};

} // namespace

void Backoff::Run(const ChainPositions &positions) {
  UTIL_TIMER("(%w s) Renormalized backoff weights\n");
  Callback callback(positions.size());
  JointOrder<Callback, PrefixOrder>(positions, callback);
}

}} // namespaces
