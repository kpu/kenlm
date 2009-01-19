#ifndef _LM_SRILanguageModel_h
#define _LM_SRILanguageModel_h

#include "LM/LanguageModel.hh"
#include "Share/Numbers.hh"

#include <Vocab.h>

#include <boost/scoped_ptr.hpp>

#include <cmath>
#include <exception>

class Ngram;

/* BIG SCARY WARNING:
 * SRI's vocabulary is not threadsafe.  Ugh.
 * No ngram length is provided.  
 *
 * Solution: use Jon's rewritten SRI LM, once he gets it stable.
 */

class SRIVocabulary : public BaseVocabulary {
	public:
		SRIVocabulary(Vocab &sri_vocab);

		~SRIVocabulary() {}

		LMWordIndex Index(const std::string &str) const {
			return Index(str.c_str());
		}

		// Returns NotFound() if the string is not in the lexicon.
		LMWordIndex Index(const char *str) const;

		const char *Word(LMWordIndex index) const;

	private:
		mutable Vocab &sri_vocab_;
};

class SRILanguageModel {
	public:
		// This LM requires no state other than history, which is externalized.
		struct State {};

		SRILanguageModel(const SRIVocabulary &vocab, const Ngram &sri_lm);

		~SRILanguageModel() {}

		const SRIVocabulary &Vocabulary() const { return vocab_; }

		State BeginSentenceState() const {
			return State();
		}

		// See the big scary warning above.

		/* ReverseHistoryIterator is an iterator such that:
		 * operator* an LMWordIndex
		 * operator++ goes _backwards_ in the sentence
		 * new_word is not in the iterators.
		 */
		template <class ReverseHistoryIterator> LogDouble IncrementalScore(
				const State &state,
				const ReverseHistoryIterator &hist_begin,
				const ReverseHistoryIterator &hist_end,
				const LMWordIndex new_word,
				unsigned int &ngram_length) const {
		        VocabIndex vocab_history[order_ + 1];
		        VocabIndex *dest = vocab_history;
			VocabIndex *dest_end = vocab_history + order_;
			ReverseHistoryIterator src = hist_begin;
			for (; (dest != dest_end) && (src != hist_end); ++dest, ++src) {
				*dest = *src;
			}
		        // If we ran out of history, pad with begin sentence.
		        for (; (dest != dest_end); ++dest) {
		                *dest = vocab_.BeginSentence();
		        }
			*dest = Vocab_None;
			return ActuallyCall(dest, new_word, ngram_length);
		}

		unsigned int Order() const { return order_; }

	private:
		LogDouble ActuallyCall(const VocabIndex *history, LMWordIndex new_word, unsigned int &ngram_length) const;

		const SRIVocabulary &vocab_;

		mutable Ngram &sri_lm_;

		const unsigned int order_;
};

inline bool operator==(const SRILanguageModel::State &left, const SRILanguageModel::State &right) {
        return true;
}

inline size_t hash_value(const SRILanguageModel::State &state) {
        return 0;
}

class SRILoadException : public LMLoadException {
	public:
		SRILoadException(const char *file_name) throw () {
			what_ = "SRILM failed to load ";
			what_ += file_name;
		}

		virtual ~SRILoadException() throw () {}

		virtual const char *what() const throw() {
			return what_.c_str();
		}

	private:
		std::string what_;
};

class SRILoader {
	public:
		SRILoader(const char *file_name, unsigned int ngram_length) throw (SRILoadException);

		~SRILoader() throw();

		const SRIVocabulary &Vocabulary() const { return vocab_; }

		const SRILanguageModel &Model() const { return model_; }

	private:
		boost::scoped_ptr<Vocab> sri_vocab_;
		boost::scoped_ptr<Ngram> sri_model_;
		SRIVocabulary vocab_;
		SRILanguageModel model_;
};

#endif
