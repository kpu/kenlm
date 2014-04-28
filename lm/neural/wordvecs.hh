#ifndef LM_NEURAL_WORDVECS_H
#define LM_NEURAL_WORDVECS_H

#include "util/scoped.hh"
#include "lm/vocab.hh"

#include <Eigen/Dense>

namespace util { class FilePiece; }

namespace lm {
namespace neural {

class WordVecs {
  public:
    // Columns of the matrix are word vectors.  The column index is the word.
    typedef Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::ColMajor> Storage;

    /* The file should begin with a line stating the number of word vectors and
     * the length of the vectors.  Then it's followed by lines containing a
     * word followed by floating-point values.
     */
    explicit WordVecs(util::FilePiece &in);

    const Storage &Vectors() const { return vecs_; }

    WordIndex Index(StringPiece str) const { return vocab_.Index(str); }

  private:
    util::scoped_malloc vocab_backing_;
    ngram::ProbingVocabulary vocab_;

    Storage vecs_;
};

}} // namespaces

#endif // LM_NEURAL_WORDVECS_H
