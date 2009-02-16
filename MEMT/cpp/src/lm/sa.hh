#ifndef LM_SA_H__
#define LM_SA_H__

#include "lm/base.hh"
#include "lm/SALM/text_length.h"
#include "util/numbers.hh"

#include <boost/functional/hash.hpp>
#include <boost/scoped_ptr.hpp>

#include <string>

class C_SingleCorpusSALM;
class C_IDVocabulary;

namespace lm {
namespace sa {

class Vocabulary : public base::Vocabulary {
	public:
		Vocabulary(const C_IDVocabulary &salm_vocab);

		WordIndex Index(const std::string &str) const;

	private:
		const C_IDVocabulary &salm_vocab_;
};

class Model {
	public:
		struct State {
			State() {}
			State(TextLenType start, unsigned char len) 
				: match_start(start), match_len(len) {}

			TextLenType match_start;
			unsigned char match_len;
		};

		Model(const Vocabulary &vocab, const C_SingleCorpusSALM &salm_lm) 
			: vocab_(vocab), salm_lm_(salm_lm) {}

		const Vocabulary &GetVocabulary() const { return vocab_; }

		State BeginSentenceState() const {
			return State(1, 1);
		}

		// This doesn't actually use history since everything SALM needs is in state.
		template <class ReverseHistoryIterator> LogDouble IncrementalScore(
				State &state,
				const ReverseHistoryIterator &hist_begin,
				const ReverseHistoryIterator &hist_end,
				const WordIndex word,
				unsigned int &ngram_length) const {
			return ActuallyCall(state, word, ngram_length);
		}

		unsigned int Order() const;

	private:
		LogDouble ActuallyCall(State &state, const WordIndex word, unsigned int &ngram_length) const;

		const Vocabulary &vocab_;
		
		const C_SingleCorpusSALM &salm_lm_;
};

inline bool operator==(const Model::State &left, const Model::State &right) {
        return (left.match_start == right.match_start) && (left.match_len == right.match_len);
}

inline size_t hash_value(const Model::State &state) {
        size_t ret = 0;
        boost::hash_combine(ret, state.match_start);
        boost::hash_combine(ret, state.match_len);
        return ret;
}

// Convenience loader that also owns everything.
class Loader {
	public:
		Loader(const char *file_name, unsigned int ngram_length);

		~Loader();

		const Vocabulary &GetVocabulary() const { return vocab_; }

		const Model &GetModel() const { return model_; }

	private:
		std::string salm_config_file_;

		// This is a pointer so I can avoid including the header here.  
		boost::scoped_ptr<C_SingleCorpusSALM> salm_;

		Vocabulary vocab_;
		Model model_;
};

} // namespace sa
} // namespace lm

#endif
