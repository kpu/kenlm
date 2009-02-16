#include "lm/sri.hh"

#include <Ngram.h>
#include <Vocab.h>

namespace lm {
namespace sri {

Vocabulary::Vocabulary(Vocab &sri_vocab) : base::Vocabulary(
		sri_vocab.getIndex(Vocab_SentStart),
		sri_vocab.getIndex(Vocab_SentEnd),
		Vocab_None,
		sri_vocab.highIndex() + 1),
	sri_vocab_(sri_vocab) {}

WordIndex Vocabulary::Index(const char *str) const {
	return sri_vocab_.getIndex(str);
}

const char *Vocabulary::Word(WordIndex index) const {
	return sri_vocab_.getWord(index);
}

Model::Model(const Vocabulary &vocab, const Ngram &sri_lm) :
	vocab_(vocab), sri_lm_(const_cast<Ngram&>(sri_lm)),
	order_((const_cast<Ngram&>(sri_lm)).setorder()) {}

LogDouble Model::ActuallyCall(SRIVocabIndex *history, const WordIndex new_word, unsigned int &ngram_length) const {
	// If you get a compiler in this function, change SRIVocabIndex in Model.hh to match the one found in SRI's Vocab.h.
	history[order_] = Vocab_None;
	const SRIVocabIndex *const_history = history;
	sri_lm_.contextID(new_word, const_history, ngram_length);
	// SRI uses log10, we use log.
	return LogDouble(AlreadyLogTag(), sri_lm_.wordProb(new_word, const_history) * M_LN10);
}

namespace {
Ngram *MakeSRIModel(const char *file_name, unsigned int ngram_length, Vocab &sri_vocab) throw (FileReadException) {
	std::auto_ptr<Ngram> ret(new Ngram(sri_vocab, ngram_length));
	File file(file_name, "r");
	if (!ret->read(file)) {
		throw FileReadException(file_name);
	}
	return ret.release();
}
} // namespace

Loader::Loader(const char *file_name, unsigned int ngram_length) throw (FileReadException) :
	sri_vocab_(new Vocab),
	sri_model_(MakeSRIModel(file_name, ngram_length, *sri_vocab_)),
	vocab_(*sri_vocab_),
	model_(vocab_, *sri_model_) {}

Loader::~Loader() throw() {}

} // namespace sri
} // namespace lm
