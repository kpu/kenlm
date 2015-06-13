#ifndef LM_INTERPOLATE_TUNE_INSTANCE_H
#define LM_INTERPOLATE_TUNE_INSTANCE_H

#include "lm/word_index.hh"
#include "util/fixed_array.hh"
#include "util/string_piece.hh"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#include <Eigen/Eigen>
#pragma GCC diagnostic pop

#include <vector>

namespace lm { namespace interpolate {

typedef Eigen::MatrixXd Matrix;
typedef Eigen::VectorXd Vector;

typedef double Accum;

// Unigram probabilities: row is word, column is model.  $unigram(x, i) = p_i(x)$
typedef Matrix UnigramProbs;

// The instance w_1^n
class Instance {
  public:
    explicit Instance(std::size_t num_models);

  private:
    friend class InstanceBuilder;

    // Pre-multiplied backoffs to unigram.
    // backoff_(i) = ln \prod_j b_i(w_j^{n-1})
    Vector backoff_;

    // correct_(i) = ln p_i(w_n | w_1^{n-1})
    // Note this is unweighted.  It appears as a term in the gradient.
    Vector correct_;

    // Correct probability values if any of the models does not back off to unigram.
    // extension_values_(i,j) = ln p_j(extension_words_[i] | w_1^{n-1})
    Matrix extension_values_;

    // Word indices corresponding to rows of extension_values_.
    std::vector<WordIndex> extension_words_;
};

void Load(int fd, const std::vector<StringPiece> &model_names, util::FixedArray<Instance> &instances, UnigramProbs &unigrams);

}} // namespaces
#endif // LM_INTERPOLATE_TUNE_INSTANCE_H
