#ifndef _LM_SRILanguageModel_h
#define _LM_SRILanguageModel_h

#include "LM/LanguageModel.hh"
#include "Share/Numbers.hh"

#include <boost/scoped_ptr.hpp>

#include <cmath>
#include <exception>

class Ngram;
class Vocab;

class HypHistory;

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

		// If there's a need for other forms of calling the LM, I might make STL-style iterators for history.
		LogDouble IncrementalScore(
				const State &state,
				const HypHistory *history,
				const LMWordIndex new_word,
				unsigned int &ngram_length) const;

		unsigned int Order() const { return order_; }

	private:
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
