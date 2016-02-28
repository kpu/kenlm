#include "lm/interpolate/tune_derivatives.hh"

#include "lm/interpolate/tune_instances.hh"
#include "lm/interpolate/tune_matrix.hh"
#include "util/stream/chain.hh"
#include "util/stream/typed_stream.hh"

#include <Eigen/Core>

namespace lm { namespace interpolate {

void Derivatives(Instances &in, Vector &weights) {
  // TODO make configurable memory size.
  util::stream::Chain chain(util::stream::ChainConfig(in.ReadExtensionsEntrySize(), 2, 64 << 20));
  chain.ActivateProgress();
  in.ReadExtensions(chain);
  util::stream::TypedStream<Extension> extensions(chain.Add());
  chain >> util::stream::kRecycle;

  // Temporaries used each cycle of the loop.
  Vector interp_uni;
  Vector unigram_cross;
  Vector full_cross;
  // Full ln p_i(x | context)
  Vector ln_p_i_full;
  Vector gradient;

  for (InstanceIndex n = 0; n < in.NumInstances(); ) {
    InstanceIndex batch_end = std::min<InstanceIndex>(n + 100, in.NumInstances());
    InstanceIndex batch_size = batch_end - n;
    // Compute p_I(x).
    interp_uni = (in.LNUnigrams() * weights).array().exp();
    // Even -inf doesn't work for <s> because weights can be negative.  Manually set it to zero.
    interp_uni(in.BOS()) = 0.0;
    Accum Z_epsilon = interp_uni.sum();
    interp_uni /= Z_epsilon;
    // unigram_cross(i) = \sum_{all x} p_I(x) ln p_i(x)
    unigram_cross = in.LNUnigrams().transpose() * interp_uni;

    gradient = in.CorrectGradientTerm() * (static_cast<float>(batch_size) / static_cast<float>(in.NumInstances()));

    for (; n < batch_end; ++n) {
      Accum weighted_backoffs = exp(in.LNBackoffs(n).dot(weights));
      // Compute \sum_{x: model does not back off to unigram} p_I(x)
      Accum sum_x_p_I = 0.0;
      // Compute \sum_{x: model does not back off to unigram} p_I(x | context)Z(context)
      Accum unnormalized_sum_x_p_I_full = 0.0;
      full_cross = Vector::Zero(weights.rows());

      // Loop over words within an instance for which extension exists.  An extension happens when any model matches more than a unigram in the tuning instance.
      while (extensions && extensions->instance == n) {
        const WordIndex word = extensions->word;
        sum_x_p_I += interp_uni(word);

        ln_p_i_full = in.LNUnigrams().row(word) + in.LNBackoffs(n);

        // Calculate ln_p_i_full(i) = ln p_i(word | context) by filling in unigrams then overwriting with extensions.
        // Loop over all models that have an extension for the same word namely p_i(word | context) matches at least a bigram.
        for (; extensions && extensions->word == word && extensions->instance == n; ++extensions) {
          ln_p_i_full(extensions->model) = extensions->ln_prob;
        }

        // This is the weighted product of probabilities.  In other words, p_I(word | context) * Z(context) = exp(\sum_i w_i * p_i(word | context)).
        Accum weighted = exp(ln_p_i_full.dot(weights));
        unnormalized_sum_x_p_I_full += weighted;

        // These aren't normalized by Z_context (happens later)
        full_cross.noalias() +=
          weighted * ln_p_i_full
          - interp_uni(word) * Z_epsilon * weighted_backoffs /* we'll divide by Z_context later to form B_I */ * in.LNUnigrams().row(word).transpose();
      }

      Accum Z_context =
        weighted_backoffs * Z_epsilon * (1.0 - sum_x_p_I) // Back off and unnormalize the unigrams for which there is no extension.
        + unnormalized_sum_x_p_I_full; // Add the extensions.
      Accum B_I = Z_epsilon / Z_context * weighted_backoffs;

      // This is the gradient term for this instance except for -log p_i(w_n | w_1^{n-1}) which was accounted for as part of neg_correct_sum_.
      // full_cross(i) is \sum_{all x} p_I(x | context) log p_i(x | context)
      // Prior terms excluded dividing by Z_context because it wasn't known at the time.
      gradient.noalias() +=
        full_cross / Z_context
        // Uncorrected term
        + B_I * (in.LNBackoffs(n).transpose() + unigram_cross)
        // Subtract values that should not have been charged.
        - sum_x_p_I * B_I * in.LNBackoffs(n).transpose();
    }
    weights -= 0.01 * gradient / batch_size;
    std::cerr << "Batch gradient is " << gradient.transpose() << " so new weights are " << weights.transpose() << std::endl;
  }
}

}} // namespaces
