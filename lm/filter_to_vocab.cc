/* Filter an ARPA language model to only contain words found in a vocabulary
 * plus <s>, </s>, and <unk>.
 */

#include "lm/filter.hh"

#include <fstream>
#include <iostream>

int main(int argc, char *argv[]) {
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " input_lm output_lm" << std::endl;
		std::cerr << "Pass the vocabulary on stdin." << std::endl;
		return 1;
	}
	
  lm::SingleVocabFilter filter(std::cin);

	std::ifstream in_lm(argv[1], std::ios::in);
  std::ofstream out_file(argv[2], std::ios::out);

  lm::FilterARPA(filter, in_lm, out_file);

	return 0;
}
