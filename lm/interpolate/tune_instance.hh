#ifndef LM_INTERPOLATE_TUNE_INSTANCE_H
#define LM_INTERPOLATE_TUNE_INSTANCE_H

#include "lm/interpolate/tune_matrix.hh"
#include "lm/word_index.hh"
#include "util/scoped.hh"
#include "util/stream/config.hh"
#include "util/string_piece.hh"

#include <boost/optional.hpp>

#include <vector>

namespace util { namespace stream {
template <class S, class T> class Sort;
class Chain;
class FileBuffer;
}} // namespaces

namespace lm { namespace interpolate {

typedef uint32_t InstanceIndex;
typedef uint32_t ModelIndex;

struct Extension {
  // Which tuning instance does this belong to?
  InstanceIndex instance;
  WordIndex word;
  ModelIndex model;
  // ln p_{model} (word | context(instance))
  float ln_prob;

  bool operator<(const Extension &other) const {
    if (instance != other.instance)
      return instance < other.instance;
    if (word != other.word)
      return word < other.word;
    if (model != other.model)
      return model < other.model;
    return false;
  }
};

class Instances {
  public:
    Instances(int tune_file, const std::vector<StringPiece> &model_names);

    Eigen::ConstRowXpr Backoffs(InstanceIndex instance) const {
      return ln_backoffs_.row(instance);
    }

    const Vector &CorrectGradientTerm() const { return neg_ln_correct_sum_; }

    const Matrix &LNUnigrams() const { return ln_unigrams_; }

    void ReadExtensions(util::stream::Chain &to);

  private:
    // backoffs_(instance, model) is the backoff all the way to unigrams.
    typedef Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor> BackoffMatrix;
    BackoffMatrix ln_backoffs_;
    
    // neg_correct_sum_(model) = -\sum_{instances} ln p_{model}(correct(instance) | context(instance)).
    // This appears as a term in the gradient.
    Vector neg_ln_correct_sum_;

    // unigrams_(word, model) = ln p_{model}(word).
    Matrix ln_unigrams_;

    struct ExtensionCompare {
      bool operator()(const void *f, const void *s) const {
        return reinterpret_cast<const Extension &>(f) < reinterpret_cast<const Extension &>(s);
      }    
    };

    // This is the source of data for the first iteration.
    util::scoped_ptr<util::stream::Sort<ExtensionCompare> > extensions_first_;

    // Source of data for subsequent iterations.  This contains already-sorted data.
    util::scoped_ptr<util::stream::FileBuffer> extensions_subsequent_;

    const util::stream::SortConfig sorting_config_;
};

}} // namespaces
#endif // LM_INTERPOLATE_TUNE_INSTANCE_H
