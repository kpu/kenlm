#include "lm/interpolate/tune_derivatives.hh"

namespace lm { namespace interpolate {

ComputeDerivative::ComputeDerivative(const util::FixedArray<Instance> &instances, const Matrix &ln_unigrams) 
  : instances_(instances), ln_unigrams_(ln_unigrams) {
  for (const Instance *i = instances.begin(); i != instances.end(); ++i) {
    neg_correct_summed_ -= i->ln_correct;
  }
}

void ComputeDerivative::Iteration(const Vector &weights, Vector &gradient, Matrix &hessian) {
  gradient = neg_correct_summed_;

  // TODO: loop instead to force low-memory evaluation
  // Compute p_I(x).
  Vector full_uni((ln_unigrams_ * weights).array().exp());
  // \sum_x p_I(x)ln p_i(x)
  Accum Z_epsilon = full_uni.sum();
  full_uni /= Z_epsilon;
  // unigram_cross(i) is the cross entropy of p_i against p_I.
  Vector unigram_cross(ln_unigrams_.transpose() * full_uni);

  Vector weighted_extensions;

  for (const Instance *n = instances_.begin(); n != instances_.end(); ++n) {
    Accum ln_weighted_backoffs = n->ln_backoff.dot(weights);
    Accum weighted_backoffs = exp(ln_weighted_backoffs);

    // Compute \sum_{x: model does not backoff to unigram} p_I(x)
    Accum sum_x_p_I = 0.0;
    for (std::vector<WordIndex>::const_iterator x = n->extension_words.begin(); x != n->extension_words.end(); ++x) {
      sum_x_p_I += full_uni(*x);
    }
    weighted_extensions = (n->ln_extensions * weights).array().exp();
    Accum Z_context = Z_epsilon * weighted_backoffs + weighted_extensions.sum() - weighted_backoffs * sum_x_p_I;

    Accum B_I = Z_epsilon / Z_context * weighted_backoffs;
    // Add uncorrected unigram term to backoff.
    gradient.noalias() += B_I * (n->ln_backoff + unigram_cross);

    // Correction term: add correct values
    gradient.noalias() += n->ln_extensions.transpose() * weighted_extensions;
    // Subtract values that should not have been charged.
    gradient -= sum_x_p_I * B_I * n->ln_backoff;
    for (std::vector<WordIndex>::const_iterator x = n->extension_words.begin(); x != n->extension_words.end(); ++x) {
      gradient.noalias() -= full_uni(*x) * B_I * ln_unigrams_.row(*x);
    }
  }
  // TODO Hessian
}

}} // namespaces
