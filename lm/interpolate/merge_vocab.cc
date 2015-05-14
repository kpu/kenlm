#include "lm/interpolate/merge_vocab.hh"

#include "lm/lm_exception.hh"
#include "util/murmur_hash.hh"

#include <string>
#include <iostream>

namespace lm {
namespace interpolate {

namespace detail {
  uint64_t HashForVocab(const char *str, std::size_t len) {
	// This proved faster than Boost's hash in speed trials: total load time Murmur 67090000, Boost 72210000
	// Chose to use 64A instead of native so binary format will be portable across 64 and 32 bit.
	return util::MurmurHash64A(str, len, 0);
  }
} // namespace detail

VocabFileReader::VocabFileReader(const int fd, const size_t model_num, uint64_t offset)
  :
  hash_value_(0),
  current_index_(0),
  eof_(false),
  model_num_(model_num),
  file_piece_(fd)
{
  StringPiece vocab_elem = file_piece_.ReadLine('\0');
  UTIL_THROW_IF(vocab_elem != "<unk>", 
                FormatLoadException,
                "Vocabulary words are in the wrong place.");
  // setup to initial value
  read();
}

bool VocabFileReader::read(void) {
  StringPiece vocab_elem;
  try {
    vocab_elem = file_piece_.ReadLine('\0');
  } catch(util::EndOfFileException &e) {
    eof_ = true;
    return !eof_;
  }
  uint64_t prev_hash_value = hash_value_;
  hash_value_ = detail::HashForVocab(vocab_elem.data(), vocab_elem.size());
      
  // hash values should be monotonically increasing
  UTIL_THROW_IF(hash_value_ < prev_hash_value, util::ErrnoException,
                ": word index not monotonically increasing."
                << " model_num: " << model_num_
                << " prev hash: " << prev_hash_value
                << " new hash: " << hash_value_);
      
  ++current_index_;
  return !eof_;
}

// constructor which takes a vector of pairs of file discriptors and model info
// This will add models to heap for heap sort and also setup the vectors
MergeVocabIndex::MergeVocabIndex(std::vector<ModelInfo>& vocab_file_info, 
                                 UniversalVocab& universal_vocab)
  : universal_vocab_(universal_vocab) {
  for (size_t i=0; i < vocab_file_info.size(); ++i) {
    file_heap_.push(new VocabFileReader(vocab_file_info[i].fd, i));

    // initialize first index to 0 for <unk>
	universal_vocab_.InsertUniversalIdx(i, 0, 0);
  }
  MergeModels();
}

// main function to do a heap sort and insert properly for each model
void MergeVocabIndex::MergeModels(void)
{
  uint64_t prev_hash_value = 0;
  // global_index starts with <unk> which is 0
  WordIndex global_index = 0;

  while (!file_heap_.empty()) {
    VocabFileReader* top_vocab_file = file_heap_.top();

	if (top_vocab_file->Value() != prev_hash_value) {
	  global_index++;
	}

	universal_vocab_.InsertUniversalIdx(top_vocab_file->ModelNum(),
                                        top_vocab_file->CurrentIndex(),
                                        global_index);

	prev_hash_value = top_vocab_file->Value();
	
	file_heap_.pop();
	if (++(*top_vocab_file)) {
	  file_heap_.push(top_vocab_file);
	}
    else {
      delete top_vocab_file;
    }
  }
}
    
} // namespace interpolate
} // namespace lm
  
