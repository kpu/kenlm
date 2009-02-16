#include "lm/sa.hh"

#include "lm/SALM/_IDVocabulary.h"
#include "lm/SALM/_SingleCorpusSALM.h"

#include <fstream>

#include <stdlib.h>
#include <err.h>

namespace lm {
namespace sa {

namespace {

// TODO: fix SALM so it has a config struct instead of requiring a file.
string AwfulInsecureSALMConfigFileHack(const char *file_name, unsigned int ngram_length) {
        string salmConfigFile("/tmp/memt-salm.cfg.");
        char tmpChars[22];
        sprintf(tmpChars, "%i", rand());
        salmConfigFile += tmpChars;
	
        //Write the temporary SALM config file:
	{	             
	        ofstream salmConfig(salmConfigFile.c_str(), ios::out);
	        salmConfig << "CORPUS\t" << file_name << "\n";
        	salmConfig << "N\t" << ngram_length << "\n";
		if (!salmConfig) err(1, "Awful SALM config hack failed.");
	}
	return salmConfigFile;
}

}  // namespace

Vocabulary::Vocabulary(const C_IDVocabulary &salm_vocab) 
	: base::Vocabulary(
			salm_vocab.returnId("_SENTENCE_START_"),
			salm_vocab.returnId("_END_OF_SENTENCE_"),
			salm_vocab.returnNullWordID(),
			salm_vocab.returnMaxID() + 1),
	salm_vocab_(salm_vocab) {}

WordIndex Vocabulary::Index(const std::string &str) const {
	return salm_vocab_.returnId(str);
}

unsigned int Model::Order() const {
	return salm_lm_.Order();
}

LogDouble Model::ActuallyCall(State &state, const WordIndex word, unsigned int &ngram_length) const {
	State current(state);
	return LogDouble(salm_lm_.LogProbAndNGramOrder(
				current.match_start,
				current.match_len,
				word,
				state.match_start,
				state.match_len,
				ngram_length), true);

}

Loader::Loader(const char *file_name, unsigned int ngram_length) 
	: salm_config_file_(AwfulInsecureSALMConfigFileHack(file_name, ngram_length)),
	  salm_(new C_SingleCorpusSALM(salm_config_file_.c_str())),
	  vocab_(salm_->GetVocabulary()),
	  model_(vocab_, *salm_) {
        //Set interpolation method:
        //  'e' = uniform mode
        //  'i' = "the way IBM suggested" (from Stephan Vogel)
        salm_->setParam_interpolationStrategy('e');
}

// Here so salm_ can be destroyed properly.
Loader::~Loader() {}

} // namespace sa
} // namespace lm
