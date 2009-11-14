#include "lm/filter.hh"

#include <boost/ptr_container/ptr_vector.hpp>

#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>

namespace lm {
namespace {

typedef boost::ptr_vector<std::string> Storage;
typedef boost::unordered_map<StringPiece, std::vector<unsigned int> > Vocabs;

// Read space separated words in enter separated lines.  
void ReadFilter(std::istream &in, PrepareMultipleVocab &out) {
	while (in >> out.TempStr()) {
		out.Insert();
		if (in.get() == '\n') out.EndSentence();
	}
}

void DisplayHelp(const char *name) {
  std::cerr
		<< "Usage: " << name << " mode input.arpa output.arpa\n\n"
		   "single mode computes the vocabulary of stdin and filters to that vocabulary.\n\n"
			 "multiple mode computes a separate vocabulary from each line of stdin.  For each line, a separate language is filtered to that line's vocabulary, with the 0-indexed line number appended to the output file name.\n\n"
			 "union mode produces one filtered model that is the union of models created by multiple mode.\n";
}

} // namespace
} // namespace lm

int main(int argc, char *argv[]) {
  if (argc < 4) {
		lm::DisplayHelp(argv[0]);
    return 1;
  }

	const char *type = argv[1], *in_name = argv[2], *out_name = argv[3];

	if (std::strcmp(type, "single") && std::strcmp(type, "union") && std::strcmp(type, "multiple")) {
		lm::DisplayHelp(argv[0]);
		return 1;
  }

  std::ifstream in_lm(in_name, std::ios::in);
	if (!in_lm) {
		err(2, "Could not open input file %s", in_name);
	}

	if (!std::strcmp(type, "single")) {
		lm::SingleVocabFilter filter(std::cin, out_name);
		lm::FilterARPA(in_lm, filter);
		return 0;
	}
	
	lm::PrepareMultipleVocab prep;
	lm::ReadFilter(std::cin, prep);

	if (!std::strcmp(type, "union")) {
		lm::MultipleVocabSingleOutputFilter filter(prep.GetVocabs(), out_name);
	  lm::FilterARPA(in_lm, filter);
	} else if (!std::strcmp(type, "multiple")) {
		lm::MultipleVocabMultipleOutputFilter filter(prep.GetVocabs(), prep.SentenceCount(), out_name);
	  lm::FilterARPA(in_lm, filter);
	}
	return 0;
}
