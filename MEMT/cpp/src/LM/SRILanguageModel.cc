#include "LM/SRILanguageModel.hh"
#include "MEMT/Decoder/History.hh"
#include "sri/include/Ngram.h"

SRILanguageModel::SRILanguageModel(const SRIVocabulary &vocab, const Ngram &sri_lm) :
	vocab_(vocab), sri_lm_(const_cast<Ngram&>(sri_lm)),
	order_((const_cast<Ngram&>(sri_lm)).setorder()) {}

LogDouble SRILanguageModel::IncrementalScore(
		const State &state,
		const HypHistory *history,
		const LMWordIndex new_word,
		unsigned int &ngram_length) const {
	VocabIndex vocab_history[order_ + 1];
	unsigned int i;
	const HypHistory *hist;
	for (i = 0, hist = history; (i < order_) && hist; hist = hist->Previous(), ++i) {
		vocab_history[i] = hist->Word();
	}
	// If we ran out of history, pad with begin sentence.
	for (; i < order_; ++i) {
		vocab_history[i] = vocab_.BeginSentence();
	}
	vocab_history[i] = Vocab_None;
	sri_lm_.contextID(new_word, vocab_history, ngram_length);
	// SRI uses log10, we use log.
	return LogDouble(sri_lm_.wordProb(new_word, vocab_history) * M_LN10, true);
}

