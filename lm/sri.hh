#ifndef LM_SRI_H__
#define LM_SRI_H__

#include "lm/base.hh"
#include "Share/Numbers.hh"

#include <boost/scoped_ptr.hpp>

#include <cmath>
#include <exception>

class Ngram;
class Vocab;

/* BIG SCARY WARNING:
 * SRI's vocabulary is not threadsafe.  Ugh.
 * The ngram length reported uses some random API I found and may be wrong.
 *
 * Solution: use Jon's rewritten SRI LM, once he gets it stable.
 */

namespace lm {
namespace sri {

class Vocabulary : public base::Vocabulary {
	public:
		Vocabulary(Vocab &sri_vocab);

		~Vocabulary() {}

		WordIndex Index(const std::string &str) const {
			return Index(str.c_str());
		}

		// Returns NotFound() if the string is not in the lexicon.
		WordIndex Index(const char *str) const;

		const char *Word(WordIndex index) const;

	private:
		mutable Vocab &sri_vocab_;
};

class Model {
	private:
		/* This should match VocabIndex found in SRI's Vocab.h
		 * The reason I define this here independently is that SRI's headers
		 * pollute and increase compile time.
		 * It's difficult to extract this from their header and anyway would
		 * break packaging.
		 * If these differ there will be a compiler error in ActuallyCall.
		 */
		typedef unsigned int SRIVocabIndex;
		
	public:
		// This LM requires no state other than history, which is externalized.
		struct State {};

		Model(const Vocabulary &vocab, const Ngram &sri_lm);

		const Vocabulary &GetVocabulary() const { return vocab_; }

		State BeginSentenceState() const {
			return State();
		}

		// See the big scary warning above.

		/* ReverseHistoryIterator is an iterator such that:
		 * operator* an WordIndex
		 * operator++ goes _backwards_ in the sentence
		 * new_word is not in the iterators.
		 */
		template <class ReverseHistoryIterator> LogDouble IncrementalScore(
				const State &state,
				const ReverseHistoryIterator &hist_begin,
				const ReverseHistoryIterator &hist_end,
				const WordIndex new_word,
				unsigned int &ngram_length) const {
		        SRIVocabIndex vocab_history[order_ + 1];
		        SRIVocabIndex *dest = vocab_history;
			SRIVocabIndex *dest_end = vocab_history + order_;
			ReverseHistoryIterator src = hist_begin;
			for (; (dest != dest_end) && (src != hist_end); ++dest, ++src) {
				*dest = *src;
			}
		        // If we ran out of history, pad with begin sentence.
		        for (; (dest != dest_end); ++dest) {
		                *dest = vocab_.BeginSentence();
		        }
			return ActuallyCall(dest, new_word, ngram_length);
		}

		unsigned int Order() const { return order_; }

	private:
		// history is an array of size order_ + 1.
		LogDouble ActuallyCall(SRIVocabIndex *history, const WordIndex new_word, unsigned int &ngram_length) const;

		const Vocabulary &vocab_;

		mutable Ngram &sri_lm_;

		const unsigned int order_;
};

inline bool operator==(const Model::State &left, const Model::State &right) {
        return true;
}

inline size_t hash_value(const Model::State &state) {
        return 0;
}

class FileReadException : public base::LoadException {
	public:
		FileReadException(const char *file_name) throw () {
			what_ = "SRILM failed to load ";
			what_ += file_name;
		}

		~FileReadException() throw () {}

		const char *what() const throw() {
			return what_.c_str();
		}

	private:
		std::string what_;
};

class Loader {
	public:
		Loader(const char *file_name, unsigned int ngram_length) throw (FileReadException);

		~Loader() throw();

		const Vocabulary &GetVocabulary() const { return vocab_; }

		const Model &GetModel() const { return model_; }

	private:
		boost::scoped_ptr<Vocab> sri_vocab_;
		boost::scoped_ptr<Ngram> sri_model_;
		Vocabulary vocab_;
		Model model_;
};

} // namespace sri
} // namespace lm

#endif
