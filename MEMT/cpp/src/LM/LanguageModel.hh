#ifndef _LM_LanguageModel_h
#define _LM_LanguageModel_h

#include "LM/WordIndex.hh"

class BaseVocabulary {
	public:
		virtual ~BaseVocabulary() {}

		LMWordIndex BeginSentence() const { return begin_sentence_; }
		LMWordIndex EndSentence() const { return end_sentence_; }
		LMWordIndex NotFound() const { return not_found_; }

		// Return start index of unused word assignments.  This is virtual until I'm
		// sure that the underlying vocabularies don't change this.
		virtual LMWordIndex Available() const = 0;

/*		// These really should be const, but I have to convince SRI of that first.
		virtual LMWordIndex Index(const char *str) const = 0;

		// Returns NULL for words not in vocabulary.  
		virtual const char *Word(LMWordIndex index) const = 0;*/
		
	protected:
		BaseVocabulary(LMWordIndex begin_sentence, LMWordIndex end_sentence, LMWordIndex not_found) :
			begin_sentence_(begin_sentence),
			end_sentence_(end_sentence),
			not_found_(not_found) {}

		const LMWordIndex begin_sentence_, end_sentence_, not_found_;
};

#endif
