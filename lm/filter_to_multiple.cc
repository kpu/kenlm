#include "lm/filter.hh"

#include <boost/ptr_container/ptr_vector.hpp>

#include <fstream>
#include <iostream>
#include <memory>

namespace lm {
namespace {

typedef boost::ptr_vector<std::string> Storage;
typedef boost::unordered_map<StringPiece, std::vector<unsigned int> > Vocabs;

void ReadFilter(std::istream &in, PrepareMultipleVocab &out) {
	// Read sentences
	while (in) {
		out.StartSentence();
		// Read words in a sentence.
		do {
			in >> out.TempStr();
			out.Insert();
		} while (in && in.peek() != '\n');
	}
}

} // namespace
} // namespace lm

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " input_lm output_lm" << std::endl;
    std::cerr << "Pass the vocabularies one per line on stdin." << std::endl;
    return 1;
  }
	lm::PrepareMultipleVocab prep;
	lm::ReadFilter(std::cin, prep);
  std::ifstream in_lm(argv[1], std::ios::in);
	lm::MultipleVocabSingleOutputFilter filter(prep.GetVocabs(), argv[2]);
	lm::FilterARPA(in_lm, filter);
	return 0;
}
