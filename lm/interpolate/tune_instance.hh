#ifndef LM_INTERPOLATE_TUNE_INSTANCE_H
#define LM_INTERPOLATE_TUNE_INSTANCE_H

#include "lm/word_index.hh"
#include "util/fixed_array.hh"
#include "util/string_piece.hh"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <Eigen/Core>
#pragma GCC diagnostic pop

#include <vector>

namespace lm { namespace interpolate {

typedef Eigen::MatrixXd Matrix;
typedef Eigen::VectorXd Vector;

typedef Matrix::Scalar Accum;

// The instance w_1^n
struct Instance {
  explicit Instance(std::size_t num_models);

  // Pre-multiplied backoffs to unigram.
  // ln_backoff(i) = ln \prod_j b_i(w_j^{n-1})
  Vector ln_backoff;

  // ln_correct(i) = ln p_i(w_n | w_1^{n-1})
  // Note this is unweighted.  It appears as a term in the gradient.
  Vector ln_correct;

  // Correct probability values if any of the models does not back off to unigram.
  // ln_extension_values(i,j) = ln p_j(extension_words[i] | w_1^{n-1})
  Matrix ln_extensions;

  // Word indices corresponding to rows of extension_values_.
  std::vector<WordIndex> extension_words;
};

// Takes ownership of tune_file.  Returns the index of <s>.
WordIndex LoadInstances(int tune_file, const std::vector<StringPiece> &model_names, util::FixedArray<Instance> &instances, Matrix &ln_unigrams);

}} // namespaces
#endif // LM_INTERPOLATE_TUNE_INSTANCE_H
