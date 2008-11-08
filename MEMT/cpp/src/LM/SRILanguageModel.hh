#ifndef _LM_SRILanguageModel_h
#define _LM_SRILanguageModel_h

#include "sri/include/Ngram.h"
#include "LM/LanguageModel.hh"

class SRIVocabulary : public BaseVocabulary {
	public:
		SRIVocabulary(Vocab &sri_vocab) 
			: BaseVocabulary(
					sri_vocab.getIndex(Vocab_SentStart), 
					sri_vocab.getIndex(Vocab_SentEnd),
					Vocab_None), 
			sri_vocab_(sri_vocab) {}

		~SRIVocabulary() {}

		LMWordIndex Available() const { return sri_vocab_.highIndex() + 1; }

		// Returns NotFound() if the string is not in the lexicon.
		LMWordIndex Index(const char *str) {
			return sri_vocab_.getIndex(str);
		}

		const char *Word(LMWordIndex index) {
			return sri_vocab_.getWord(index);
		}

	private:
		Vocab &sri_vocab_;
};

class SRILanguageModel {
	public:
		// This LM requires no state other than history, which is externalized.
		struct State {};

		SRILanguageModel(SRIVocabulary &vocab, Ngram &sri_lm)
			: vocab_(vocab), sri_lm_(sri_lm), order_(sri_lm.setorder()) {}

		~SRILanguageModel() {}

		SRIVocabulary &Vocabulary() { return vocab_; }

		State BeginSentenceState() const {
			return State();
		}

		LogDouble IncrementalScoreReversed(
				State &state,
				const vector<LMWordIndex> &words,
				unsigned int *current_ngram) const {
			// Sadly, we don't have ngram sizes from SRI yet.
			*current_ngram = 1;
			unsigned int history_size = min<unsigned int>(words.size(), order_);
			VocabIndex history[history_size];
			for (int i = 0; i < history_size - 1; ++i) {
				history[i] = words[i+1];
			}
			history[history_size - 1] = Vocab_None;
			// Convert from SRI's log10 to ln.
			return LogDouble(sri_lm_.wordProb(words[0], history) * M_LN10, true);
		}

		unsigned int Order() const { return order_; }

	private:
		// This should be const, but SRI is missing some const markers.
		SRIVocabulary &vocab_;

		Ngram &sri_lm_;

		const unsigned int order_;
};

#endif
