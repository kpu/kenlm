#include "lm/interpolate/tune_derivatives.hh"

namespace lm { namespace interpolate {

ComputeDerivative::ComputeDerivative(const util::FixedArray<Instance> &instances, const Matrix &ln_unigrams, WordIndex bos)
  : instances_(instances), ln_unigrams_(ln_unigrams), bos_(bos) {
  neg_correct_summed_ = Vector::Zero(ln_unigrams.cols());
  for (const Instance *i = instances.begin(); i != instances.end(); ++i) {
    neg_correct_summed_ -= i->ln_correct;
  }
}

Accum ComputeDerivative::Iteration(const Vector &weights, Vector &gradient, Matrix &hessian) {
  gradient = neg_correct_summed_;
  hessian = Matrix::Zero(weights.rows(), weights.rows());

  // TODO: loop instead to force low-memory evaluation
  // Compute p_I(x).
  Vector interp_uni((ln_unigrams_ * weights).array().exp());
  // Even -inf doesn't work for <s> because weights can be negative.  Manually set it to zero.
  interp_uni(bos_) = 0.0;
  Accum Z_epsilon = interp_uni.sum();
  interp_uni /= Z_epsilon;
  // unigram_cross(i) = \sum_{all x} p_I(x) ln p_i(x)
  Vector unigram_cross(ln_unigrams_.transpose() * interp_uni);

  Accum sum_B_I = 0.0;
  Accum sum_ln_Z_context = 0.0;

  Vector weighted_extensions;
  Matrix convolve;
  Vector full_cross;

  for (const Instance *n = instances_.begin(); n != instances_.end(); ++n) {
    Accum ln_weighted_backoffs = n->ln_backoff.dot(weights);
    Accum weighted_backoffs = exp(ln_weighted_backoffs);

    // Compute \sum_{x: model does not backoff to unigram} p_I(x)
    Accum sum_x_p_I = 0.0;
    for (std::vector<WordIndex>::const_iterator x = n->extension_words.begin(); x != n->extension_words.end(); ++x) {
      sum_x_p_I += interp_uni(*x);
    }
    weighted_extensions = (n->ln_extensions * weights).array().exp();
    Accum Z_context = Z_epsilon * weighted_backoffs * (1.0 - sum_x_p_I) + weighted_extensions.sum();
    sum_ln_Z_context += log(Z_context);

    Accum B_I = Z_epsilon / Z_context * weighted_backoffs;
    sum_B_I += B_I;

    // This is the gradient term for this instance except for -log p_i(w_n | w_1^{n-1}) which was accounted for as part of neg_correct_sum_.
    // full_cross(i) is \sum_{all x} p_I(x | context) log p_i(x | context)
    full_cross =
      // Uncorrected term
      B_I * (n->ln_backoff + unigram_cross)
      // Correction term: add correct values
      + n->ln_extensions.transpose() * weighted_extensions / Z_context
      // Subtract values that should not have been charged.
      - sum_x_p_I * B_I * n->ln_backoff;
    for (std::vector<WordIndex>::const_iterator x = n->extension_words.begin(); x != n->extension_words.end(); ++x) {
      full_cross.noalias() -= interp_uni(*x) * B_I * ln_unigrams_.row(*x);
    }

    gradient += full_cross;

    convolve = unigram_cross * n->ln_backoff.transpose();
    // There's one missing term here, which is independent of context and done at the end.
    hessian.noalias() +=
      // First term of Hessian, assuming all models back off to unigram.
      B_I * (convolve + convolve.transpose() + n->ln_backoff * n->ln_backoff.transpose())
      // Second term of Hessian, with correct full probabilities.
      - full_cross * full_cross.transpose();

    // Adjust the first term of the Hessian to account for extension
    for (std::size_t x = 0; x < n->extension_words.size(); ++x) {
      WordIndex universal_x = n->extension_words[x];
      hessian.noalias() +=
        // Replacement terms.
        weighted_extensions(x) / Z_context * n->ln_extensions.row(x).transpose() * n->ln_extensions.row(x)
        // Presumed unigrams.  TODO: individual terms with backoffs pulled out?  Maybe faster?
        - interp_uni(universal_x) * B_I * (ln_unigrams_.row(universal_x).transpose() + n->ln_backoff) * (ln_unigrams_.row(universal_x) + n->ln_backoff.transpose());
    }
  }

  for (Matrix::Index x = 0; x < interp_uni.rows(); ++x) {
    // \sum_{contexts} B_I(context) \sum_x p_I(x) log p_i(x) log p_j(x)
    hessian.noalias() += sum_B_I * interp_uni(x) * ln_unigrams_.row(x).transpose() * ln_unigrams_.row(x);
  }
  return exp((neg_correct_summed_.dot(weights) + sum_ln_Z_context) / static_cast<double>(instances_.size()));
}

}} // namespaces
