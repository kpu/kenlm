#include "lm/interpolate/universal_vocab.hh"

namespace lm {
  namespace interpolate {

    UniversalVocab::UniversalVocab(const std::vector<WordIndex>& model_max_idx)
    {
        model_index_map_.reserve(model_max_idx.size());
        for (size_t i = 0; i < model_max_idx.size(); ++i) {
            model_index_map_[i].reserve(model_max_idx[i]);
      }
    }

    void UniversalVocab::InsertUniversalIdx(size_t model_num, WordIndex word_index,
                                            WordIndex universal_word_index)
    {
      model_index_map_[model_num][word_index] = universal_word_index;
    }

  }} // namespaces
