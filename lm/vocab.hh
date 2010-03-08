#ifndef LM_VOCAB_H__
#define LM_VOCAB_H__

#include "lm/exception.hh"
#include "lm/word_index.hh"

#include <boost/unordered_map.hpp>
#include <boost/ptr_container/ptr_vector.hpp>

#include <memory>

namespace lm {

/* This doesn't inherit from Vocabulary so it can be used where the special
 * tags are not applicable.   
 * TODO: make ngram.* and SALM use this.
 */
class GenericVocabulary {
	public:
    static const WordIndex kNotFoundIndex = 0;
    static const char *kNotFoundWord;

	  GenericVocabulary() {
      strings_.push_back(new std::string(kNotFoundIndex));
      ids_[strings_[0]] = kNotFoundIndex;
      strings_.push_back(new std::string());
      available_ = 1;
    }

    /* Query API */

		WordIndex Index(const std::string &str) const {
			return Index(StringPiece(str));
		}

		WordIndex Index(const StringPiece &str) const {
			boost::unordered_map<StringPiece, WordIndex>::const_iterator i(ids_.find(str));
			return (__builtin_expect(i == ids_.end(), 0)) ? kNotFoundIndex : i->second;
		}

    bool Known(const StringPiece &str) const {
      return ids_.find(str) != ids_.end();
    }

		const char *Word(WordIndex index) const {
			return strings_[index].c_str();
		}

    /* Insertion API */

		void Reserve(size_t to) {
			strings_.reserve(to);
			ids_.rehash(to + 1);
		}

    std::string &Temp() {
      return strings_.back();
    }

    // Take the string returned by Temp() and insert it.
    WordIndex InsertOrFind() {
			std::pair<boost::unordered_map<StringPiece, WordIndex>::const_iterator, bool> res(ids_.insert(std::make_pair(StringPiece(strings_.back()), available_)));
      if (res.second) {
        ++available_;
        strings_.push_back(new std::string());
      }
      return res.first->second;
    }

    // Insert a word.  Throw up if already found.  Take ownership of the word in either case.  
		WordIndex InsertOrThrow() throw(WordDuplicateVocabLoadException) {
			std::pair<boost::unordered_map<StringPiece, WordIndex>::const_iterator, bool> res(ids_.insert(std::make_pair(StringPiece(strings_.back()), available_)));
			if (!res.second) {
				throw WordDuplicateVocabLoadException(strings_.back(), res.first->second, available_);
			}
      ++available_;
			strings_.push_back(new std::string());
      return res.first->second;
		}

	private:
		// TODO: optimize memory use here by using one giant buffer, preferably premade by a binary file format.
		boost::ptr_vector<std::string> strings_;
		boost::unordered_map<StringPiece, WordIndex> ids_;

    WordIndex available_;
};

const char *GenericVocabulary::kNotFoundWord = "<unk>";

} // namespace lm

#endif // LM_VOCAB_H__
