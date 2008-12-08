#ifndef _LM_SRILanguageModel_h
#define _LM_SRILanguageModel_h

#include <cmath>
#include "sri/include/Ngram.h"
#include "LM/LanguageModel.hh"

/* BIG SCARY WARNING:
 * SRI's vocabulary is not threadsafe.  Ugh.
 * No ngram length is provided.  
 *
 * Solution: use Jon's rewritten SRI LM, once he gets it stable.
 */

class SRIVocabulary : public BaseVocabulary {
	public:
		SRIVocabulary(Vocab &sri_vocab) 
			: BaseVocabulary(
					sri_vocab.getIndex(Vocab_SentStart), 
					sri_vocab.getIndex(Vocab_SentEnd),
					Vocab_None,
					sri_vocab.highIndex() + 1), 
			sri_vocab_(sri_vocab) {}

		~SRIVocabulary() {}

		// Returns NotFound() if the string is not in the lexicon.
		LMWordIndex Index(const char *str) const {
			return sri_vocab_.getIndex(str);
		}

		const char *Word(LMWordIndex index) const {
			return sri_vocab_.getWord(index);
		}

	private:
		mutable Vocab &sri_vocab_;
};

class SRILanguageModel {
	public:
		// This LM requires no state other than history, which is externalized.
		struct State {};

		SRILanguageModel(const SRIVocabulary &vocab, const Ngram &sri_lm)
			: vocab_(vocab), sri_lm_(const_cast<Ngram&>(sri_lm)), order_((const_cast<Ngram&>(sri_lm)).setorder()) {}

		~SRILanguageModel() {}

		const SRIVocabulary &Vocabulary() const { return vocab_; }

		State BeginSentenceState() const {
			return State();
		}

		// In practice LinkedHistory is always HypHistory, but I didn't want to make this dependency explicit.
		// If there's a need for other forms of calling the LM, I might make STL-style iterators for history.
		template <class LinkedHistory> LogDouble IncrementalScore(
				const State &state,
				const LinkedHistory *history,
				const LMWordIndex new_word,
				unsigned int *ngram_length) const {
			*ngram_length = 0;
			VocabIndex vocab_history[order_];
			unsigned int i;
			const LinkedHistory *hist;
			for (i = 0, hist = history; (i < order_ - 1) && hist; hist = hist->Previous(), ++i) {
				vocab_history[i] = hist->Word();
			}
			// If we ran out of history, pad with begin sentence.
			if (!hist) {
				for (; i < order_ - 1; ++i) {
					vocab_history[i] = vocab_.BeginSentence();
				}
			}
			vocab_history[i] = Vocab_None;
			// SRI uses log10, we use log.
			return LogDouble(sri_lm_.wordProb(new_word, vocab_history) * M_LN10, true);
		}

		unsigned int Order() const { return order_; }

	private:
		const SRIVocabulary &vocab_;

		mutable Ngram &sri_lm_;

		const unsigned int order_;
};

#endif
