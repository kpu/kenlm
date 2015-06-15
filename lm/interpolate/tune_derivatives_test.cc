#include "lm/interpolate/tune_derivatives.hh"

#include "lm/interpolate/tune_instance.hh"

#define BOOST_TEST_MODULE DerivativeTest
#include <boost/test/unit_test.hpp>

namespace lm { namespace interpolate { namespace {

BOOST_AUTO_TEST_CASE(Small) {
  // Three vocabulary words plus <s>, two models.
  Matrix unigrams(4, 2);
  unigrams <<
    0.1, 0.6,
    0.4, 0.3,
    0.5, 0.1,
    // <s>
    1.0, 1.0;
  unigrams = unigrams.array().log();

  // One instance
  util::FixedArray<Instance> instances(1);
  instances.push_back(2);
  Instance &instance = instances.back();

  instance.ln_backoff << 0.2, 0.4;
  instance.ln_backoff = instance.ln_backoff.array().log();

  // Sparse cases: model 0 word 2 and model 1 word 1.

  // Assuming that model 1 only matches word 1, this is p_1(1 | context)
  Accum model_1_word_1 = 1.0 - .6 * .4 - .1 * .4;

  // We'll suppose correct has WordIndex 1, which backs off in model 0, and matches in model 1
  instance.ln_correct << (0.4 * 0.2), model_1_word_1;
  instance.ln_correct = instance.ln_correct.array().log();

  Accum model_0_word_2 = 1.0 - .1 * .2 - .4 * .2;

  instance.extension_words.push_back(1);
  instance.extension_words.push_back(2);
  instance.ln_extensions.resize(2, 2);
  instance.ln_extensions <<
    (0.4 * 0.2), model_1_word_1,
    model_0_word_2, 0.1 * 0.4;
  instance.ln_extensions = instance.ln_extensions.array().log();

  ComputeDerivative compute(instances, unigrams, 3);
  Vector weights(2);
  weights << 0.9, 1.2;

  Vector gradient(2);
  Matrix hessian(2,2);
  compute.Iteration(weights, gradient, hessian);

  // p_I(x | context)
  Vector p_I(3);
  p_I <<
    pow(0.1 * 0.2, 0.9) * pow(0.6 * 0.4, 1.2),
    pow(0.4 * 0.2, 0.9) * pow(model_1_word_1, 1.2),
    pow(model_0_word_2, 0.9) * pow(0.1 * 0.4, 1.2);
  p_I /= p_I.sum();

  Vector expected_gradient = -instance.ln_correct;
  expected_gradient(0) += p_I(0) * log(0.1 * 0.2);
  expected_gradient(0) += p_I(1) * log(0.4 * 0.2);
  expected_gradient(0) += p_I(2) * log(model_0_word_2);
  BOOST_CHECK_CLOSE(expected_gradient(0), gradient(0), 0.01);

  expected_gradient(1) += p_I(0) * log(0.6 * 0.4);
  expected_gradient(1) += p_I(1) * log(model_1_word_1);
  expected_gradient(1) += p_I(2) * log(0.1 * 0.4);
  BOOST_CHECK_CLOSE(expected_gradient(1), gradient(1), 0.01);

  Matrix expected_hessian(2, 2);
  expected_hessian(1, 0) =
    // First term
    p_I(0) * log(0.1 * 0.2) * log(0.6 * 0.4) +
    p_I(1) * log(0.4 * 0.2) * log(model_1_word_1) +
    p_I(2) * log(model_0_word_2) * log(0.1 * 0.4);
  expected_hessian(1, 0) -=
    (p_I(0) * log(0.1 * 0.2) + p_I(1) * log(0.4 * 0.2) + p_I(2) * log(model_0_word_2)) *
    (p_I(0) * log(0.6 * 0.4) + p_I(1) * log(model_1_word_1) + p_I(2) * log(0.1 * 0.4));
  expected_hessian(0, 1) = expected_hessian(1, 0);
  BOOST_CHECK_CLOSE(expected_hessian(1, 0), hessian(1, 0), 0.01);
  BOOST_CHECK_CLOSE(expected_hessian(0, 1), hessian(0, 1), 0.01);
}

}}} // namespaces
