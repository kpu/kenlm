#ifndef LM_INTERPOLATE_UNIVERSAL_VOCAB_H
#define LM_INTERPOLATE_UNIVERSAL_VOCAB_H

#include "lm/word_index.hh"

#include <vector>

namespace lm {
namespace interpolate {

  class UniversalVocab {
  public:
      typedef std::vector<std::vector<WordIndex> > HashMapType;
      
  public:
    UniversalVocab(const std::vector<WordIndex>& model_max_idx);

    // GetUniversalIndex takes the model numberand index for the specific
    // model and returns the universal model number
    // If you are outside of vocabulary size this will return 0
    WordIndex GetUniversalIdx(size_t model_num, WordIndex model_word_index) const
    {
/*
      HashMapType::const_iterator iter = model_index_map_.find(model_num);
      if (iter != model_index_map_.end()) {
          return (iter->second)[model_word_index];
      }
*/
      return 0;
    }

    // private:
      
    void InsertUniversalIdx(size_t model_num, WordIndex word_index,
                            WordIndex universal_word_index);
      
  private:
    HashMapType model_index_map_;
  };
    
} // namespace interpolate
} // namespace lm

#endif // LM_INTERPOLATE_UNIVERSAL_VOCAB_H
