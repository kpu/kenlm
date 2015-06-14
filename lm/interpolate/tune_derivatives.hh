#ifndef LM_INTERPOLATE_TUNE_DERIVATIVES_H
#define LM_INTERPOLATE_TUNE_DERIVATIVES_H

#include "lm/interpolate/tune_instance.hh"

namespace lm { namespace interpolate {

class ComputeDerivative {
  public:
    explicit ComputeDerivative(const util::FixedArray<Instance> &instances, const UnigramProbs &unigrams);

    void Iteration(const Vector &weights, Vector &gradient, Matrix &hessian);

  private:
    const util::FixedArray<Instance> &instances_;
    const UnigramProbs &unigrams_;

    // correct_summed_(i) = -\sum_n ln p_i(w_n | w_1^{n-1})
    Vector neg_correct_summed_;
};

}} // namespaces

#endif // LM_INTERPOLATE_TUNE_DERIVATIVES_H

