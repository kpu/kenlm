#include "lm/interpolate/merge_vocab.hh"

#include "lm/lm_exception.hh"
#include "util/file_piece.hh"
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
      fd_(fd),
      file_piece_ptr_(new util::FilePiece(fd)),
      model_num_(model_num)
    {
      StringPiece vocab_elem = file_piece_ptr_->ReadLine('\0');
      UTIL_THROW_IF(
		    memcmp(vocab_elem.data(), "<unk>", 6),
		    FormatLoadException,
		    "Vocabulary words are in the wrong place.");

      // NOTE: for debugging remove
      word_str_ = std::string(vocab_elem.data());
      // setup to initial value
      read();
    }

    bool VocabFileReader::read(void)
    {
      StringPiece vocab_elem;
      try {
	vocab_elem = file_piece_ptr_->ReadLine('\0');
      }
      // NOTE: should only catch correct throw.
      catch(util::Exception &e) {
	return false;
      }
      // NOTE: this is just for debugging
      word_str_ = vocab_elem.data();
      uint64_t prev_hash_value = hash_value_;
      hash_value_ = detail::HashForVocab(vocab_elem.data(), vocab_elem.size());
      
      // hash values should be monotonically increasing
      UTIL_THROW_IF(hash_value_ <= prev_hash_value, util::ErrnoException,
		    ": word index not monotonically increasing."
		    << " model_num: " << model_num_
		    << " string: " << GetString()
		    << " prev hash: " << prev_hash_value
		    << " new hash: " << hash_value_);
      
      ++current_index_;
      return true;

    }

    // constructor which takes a vector of pairs of file discriptors and model info
    // This will add models to heap for heap sort and also setup the vectors
    MergeVocabIndex::MergeVocabIndex(std::vector<ModelInfo>& vocab_file_info, UniversalVocab& universal_vocab)
      : universal_vocab_(universal_vocab)
    {
      for (size_t i=0; i < vocab_file_info.size(); ++i) {
	VocabFileReader vocab_file(vocab_file_info[i].fd, i);
	file_heap_.push(vocab_file);

	// initialize first index to 0 for <unk>
	universal_vocab_.InsertUniversalIdx(i, 0, 0);
      }

      MergeModels();
    }

    // main function to do a heap sort and insert properly for each model
    void MergeVocabIndex::MergeModels(void)
    {
      uint64_t prev_hash_value = 0;
      // global_index starts at 1 because of <unk> which is 0
      WordIndex global_index = 0;

      while (!file_heap_.empty()) {
	VocabFileReader top_vocab_file = file_heap_.top();

	if (top_vocab_file.Value() != prev_hash_value) {
	  global_index++;
	}

	universal_vocab_.InsertUniversalIdx(top_vocab_file.ModelNum(),
					    top_vocab_file.CurrentIndex(),
					    global_index);

	prev_hash_value = top_vocab_file.Value();
	
	file_heap_.pop();
	// if (top_vocab_file.read_deprecated()) {
	if (top_vocab_file.read()) {
	  file_heap_.push(top_vocab_file);
	}
      }
    }
    
  } // namespace interpolate
} // namespace lm
  
