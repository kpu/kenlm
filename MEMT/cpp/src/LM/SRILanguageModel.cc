#include "LM/SRILanguageModel.hh"

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

LogDouble SRILanguageModel::ActuallyCall(SRIVocabIndex *history, const LMWordIndex new_word, unsigned int &ngram_length) const {
	// If you get a compiler in this function, change SRIVocabIndex in SRILanguageModel.hh to match the one found in SRI's Vocab.h.
	history[order_] = Vocab_None;
	const SRIVocabIndex *const_history = history;
	sri_lm_.contextID(new_word, const_history, ngram_length);
	// SRI uses log10, we use log.
	return LogDouble(sri_lm_.wordProb(new_word, const_history) * M_LN10, true);
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
