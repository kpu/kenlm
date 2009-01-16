#include "LM/SRILanguageModel.hh"
#include "MEMT/Decoder/History.hh"

#include <Ngram.h>
#include <Vocab.h>

SRIVocabulary::SRIVocabulary(Vocab &sri_vocab) : BaseVocabulary(
		sri_vocab.getIndex(Vocab_SentStart),
		sri_vocab.getIndex(Vocab_SentEnd),
		Vocab_None,
		sri_vocab.highIndex() + 1),
	sri_vocab_(sri_vocab) {}

LMWordIndex SRIVocabulary::Index(const char *str) const {
	return sri_vocab_.getIndex(str);
}

const char *SRIVocabulary::Word(LMWordIndex index) const {
	return sri_vocab_.getWord(index);
}

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
	// In theory, it shouldn't matter which previous word is used.
	for (i = 0, hist = history; (i < order_) && hist; hist = hist->AnyPrevious(), ++i) {
		vocab_history[i] = hist->Entry().word;
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

namespace {
Ngram *MakeSRIModel(const char *file_name, unsigned int ngram_length, Vocab &sri_vocab) throw (SRILoadException) {
	std::auto_ptr<Ngram> ret(new Ngram(sri_vocab, ngram_length));
	File file(file_name, "r");
	if (!ret->read(file)) {
		throw SRILoadException(file_name);
	}
	return ret.release();
}
} // namespace

SRILoader::SRILoader(const char *file_name, unsigned int ngram_length) throw (SRILoadException) :
	sri_vocab_(new Vocab),
	sri_model_(MakeSRIModel(file_name, ngram_length, *sri_vocab_)),
	vocab_(*sri_vocab_),
	model_(vocab_, *sri_model_) {}

SRILoader::~SRILoader() throw() {}
