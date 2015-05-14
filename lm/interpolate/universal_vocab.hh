#ifndef LM_INTERPOLATE_UNIVERSAL_VOCAB_H
#define LM_INTERPOLATE_UNIVERSAL_VOCAB_H

#include "lm/word_index.hh"

#include <vector>
#include <cstddef>

namespace lm {
namespace interpolate {

class UniversalVocab {
public:
  explicit UniversalVocab(const std::vector<WordIndex>& model_max_idx);

  // GetUniversalIndex takes the model numberand index for the specific
  // model and returns the universal model number
  // If you are outside of vocabulary size this will return 0
  WordIndex GetUniversalIdx(size_t model_num, WordIndex model_word_index) const {
    if (model_num < model_index_map_.size()) {
      return model_index_map_[model_num][model_word_index];
    }
    return 0;
  }

  void InsertUniversalIdx(size_t model_num, WordIndex word_index,
      WordIndex universal_word_index) {
    model_index_map_[model_num][word_index] = universal_word_index;
  }

private:
  std::vector<std::vector<WordIndex> > model_index_map_;
};
    
} // namespace interpolate
} // namespace lm

#endif // LM_INTERPOLATE_UNIVERSAL_VOCAB_H
