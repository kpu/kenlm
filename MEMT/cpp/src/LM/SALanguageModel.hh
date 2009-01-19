#ifndef _LM_SALanguageModel_h
#define _LM_SALanguageModel_h

#include "Share/Log.hh"
#include "LM/LanguageModel.hh"
#include "LM/SALM/text_length.h"

#include <boost/functional/hash.hpp>
#include <boost/scoped_ptr.hpp>

#include <string>

class C_SingleCorpusSALM;
class C_IDVocabulary;

class SAVocabulary : public BaseVocabulary {
	public:
		SAVocabulary(const C_IDVocabulary &salm_vocab);

		LMWordIndex Index(const std::string &str) const;

	private:
		const C_IDVocabulary &salm_vocab_;
};

class SALanguageModel {
	public:
		struct State {
			State() {}
			State(TextLenType start, unsigned char len) 
				: match_start(start), match_len(len) {}

			TextLenType match_start;
			unsigned char match_len;
		};

		SALanguageModel(const SAVocabulary &vocab, const C_SingleCorpusSALM &salm_lm) 
			: vocab_(vocab), salm_lm_(salm_lm) {}

		const SAVocabulary &Vocabulary() const { return vocab_; }

		State BeginSentenceState() const {
			return State(1, 1);
		}

		// This doesn't actually use history since everything SALM needs is in state.
		template <class ReverseHistoryIterator> LogDouble IncrementalScore(
				State &state,
				const ReverseHistoryIterator &hist_begin,
				const ReverseHistoryIterator &hist_end,
				const LMWordIndex word,
				unsigned int &ngram_length) const {
			return ActuallyCall(state, word, ngram_length);
		}

		unsigned int Order() const;

	private:
		LogDouble ActuallyCall(State &state, const LMWordIndex word, unsigned int &ngram_length) const;

		const SAVocabulary &vocab_;
		
		const C_SingleCorpusSALM &salm_lm_;
};

inline bool operator==(const SALanguageModel::State &left,
                const SALanguageModel::State &right) {
        return (left.match_start == right.match_start) && (left.match_len == right.match_len);
}

inline size_t hash_value(const SALanguageModel::State &state) {
        size_t ret = 0;
        boost::hash_combine(ret, state.match_start);
        boost::hash_combine(ret, state.match_len);
        return ret;
}

// Convenience loader that also owns everything.
class SALoader {
	public:
		SALoader(const char *file_name, unsigned int ngram_length);

		~SALoader();

		const SAVocabulary &Vocabulary() const { return vocab_; }

		const SALanguageModel &Model() const { return model_; }

	private:
		std::string salm_config_file_;

		// This is a pointer so I can avoid including the header here.  
		boost::scoped_ptr<C_SingleCorpusSALM> salm_;

		SAVocabulary vocab_;
		SALanguageModel model_;
};

#endif // _LM_SALanguageModel_h
