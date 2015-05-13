#ifndef LM_INTERPOLATE_MERGE_VOCAB_H
#define LM_INTERPOLATE_MERGE_VOCAB_H

#include "lm/word_index.hh"
#include "lm/interpolate/universal_vocab.hh"

#include <queue>
#include <stdint.h>
#include <vector>

#include <boost/unordered_map.hpp>

namespace util { class FilePiece;  }

namespace lm {
namespace interpolate {

  struct ModelInfo
  {
    int fd;
    size_t vocab_size;
  };

  class VocabFileReader
  {
  public:
    explicit VocabFileReader(const int fd, size_t model_num, uint64_t offset =0);

    bool read(void);
    bool read_deprecated(void);

    uint64_t operator()(void) const { return Value(); }
    uint64_t Value(void) const { return hash_value_; }

    // Functions below are for debugging
    uint32_t ModelNum(void) const { return model_num_; }
    WordIndex CurrentIndex(void) const { return current_index_; }
    std::string GetString(void) const { return word_str_; }

  private:
    uint64_t hash_value_;
    WordIndex current_index_;
      
    int fd_;
    util::FilePiece* file_piece_ptr_;
    size_t model_num_;
    std::string word_str_;
  };


  class CompareFiles
  {
  public:
    bool operator()(const VocabFileReader& x,
		    const VocabFileReader& y)
    { return x()>y(); }
  };

  class MergeVocabIndex
  {
  private:
    typedef std::priority_queue<VocabFileReader, std::vector<VocabFileReader>,
				CompareFiles> HeapType;
      
    typedef boost::unordered_map<size_t, std::vector<WordIndex> > HashMapType;
  public:
    explicit MergeVocabIndex(std::vector<ModelInfo>& vocab_file_info,
			     UniversalVocab& universam_vocab);

    void MergeModels(void);

    WordIndex GetUniversalIndex(size_t model_num, 
				WordIndex original_index);

  private:
    UniversalVocab& universal_vocab_;
      
    HeapType    file_heap_;

  };
}} // namespaces
      
#endif // LM_INTERPOLATE_MERGE_VOCAB_H
