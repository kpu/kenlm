#include "LM/SALMLanguageModel.hh"
#include <stdlib.h>
#include <err.h>
#include <fstream>

using namespace std;

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

SALMLoader::SALMLoader(const char *file_name, unsigned int ngram_length) 
	: salm_config_file_(AwfulInsecureSALMConfigFileHack(file_name, ngram_length)),
	  salm_(salm_config_file_.c_str()),
	  vocab_(salm_.GetVocabulary()),
	  model_(vocab_, salm_) {
        //Set interpolation method:
        //  'e' = uniform mode
        //  'i' = "the way IBM suggested" (from Stephan Vogel)
        salm_.setParam_interpolationStrategy('e');
}
