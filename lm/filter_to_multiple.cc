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
	for (unsigned int sent = 0; in; ++sent) {
		out.StartSentence(sent);
		// Read words in a sentence.
		while (in) {
			in >> out.TempStr();
			out.Insert();
			if (in.peek() == '\n') break;
		}
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
  std::ofstream out_lm(argv[2], std::ios::out);

	lm::FilterARPA(prep.Filter(), in_lm, out_lm);
	return 0;
}
