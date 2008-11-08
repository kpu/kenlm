#ifndef _LM_SALMLanguageModel_h
#define _LM_SALMLanguageModel_h

#include "Share/Log.hh"
#include "LM/LanguageModel.hh"
#include "LM/SALM/_IDVocabulary.h"
#include "LM/SALM/_SingleCorpusSALM.h"

class SALMVocabulary : public BaseVocabulary {
	public:
		SALMVocabulary(const C_IDVocabulary &salm_vocab) :
			BaseVocabulary(
					salm_vocab.returnId(C_String("_SENTENCE_START_")),
					salm_vocab.returnId(C_String("_END_OF_SENTENCE_")),
					salm_vocab.returnNullWordID()),
			salm_vocab_(salm_vocab) {}

		LMWordIndex Available() const {
			return salm_vocab_.returnMaxID() + 1;
		}

		LMWordIndex Index(const char *str) const {
			return salm_vocab_.returnId(C_String(str));
		}

		const char *Word(LMWordIndex index) const {
			return salm_vocab_.getText(index).toString();
		}

	private:
		const C_IDVocabulary &salm_vocab_;
};

class SALMLanguageModel {
	public:
		struct State {
			State() {}
			State(TextLenType start, unsigned char len) 
				: match_start(start), match_len(len) {}

			TextLenType match_start;
			unsigned char match_len;
		};

		SALMLanguageModel(const SALMVocabulary &vocab, const C_SingleCorpusSALM &salm_lm) 
			: vocab_(vocab), salm_lm_(salm_lm) {}

		const SALMVocabulary &Vocabulary() const { return vocab_; }

		State BeginSentenceState() const {
			return State(1, 1);
		}

		LogDouble IncrementalScoreReversed(
				State &state,
				const vector<LMWordIndex> &words,
				unsigned int *current_ngram) const {
			assert(!words.empty());
			State current(state);
			// TODO(kheafiel): edit SALM to avoid much duplicated computation.
			*current_ngram = salm_lm_.NGramOrder(
					current.match_start,
					current.match_len,
					words[0],
					state.match_start,
					state.match_len);
			return LogDouble(salm_lm_.LogProb(
						current.match_start,
						current.match_len,
						words[0],
						state.match_start,
						state.match_len), true);
		}

		unsigned int Order() const {
			return salm_lm_.Order();
		}

	private:
		const SALMVocabulary &vocab_;
		
		const C_SingleCorpusSALM &salm_lm_;
};

#endif // _LM_SALMLanguageModel_h
