/* Filter an ARPA language model to only contain words found in a vocabulary
 * plus <s>, </s>, and <unk>.
 */

#include "lm/filter.hh"

#include "util/null_intersection.hh"
#include "util/string_piece.hh"
#include "util/tokenize_piece.hh"

#include <boost/lexical_cast.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>

#include <istream>
#include <ostream>
#include <memory>
#include <string>
#include <vector>

#include <err.h>
#include <string.h>

namespace lm {

SingleVocabFilter::SingleVocabFilter(std::istream &in) {
  // Insert SRI's custom words.
  words_.insert("<s>");
  words_.insert("</s>");
  words_.insert("<unk>");
  words_.insert("<UNK>");
  std::auto_ptr<std::string> word(new std::string());
  while (in >> *word) {
    if (words_.insert(StringPiece(*word)).second) {
      backing_.push_back(word);
      word.reset(new std::string());
    }
  }
  if (!in.eof()) err(1, "Reading text from stdin");
}

OutputLM::OutputLM(std::ostream &file, std::streampos max_count_space) : file_(file), max_count_space_(max_count_space) {
  file_.exceptions(std::ostream::eofbit | std::ostream::failbit | std::ostream::badbit);
 	for (std::streampos i = 0; i < max_count_space; i += std::streampos(1)) {
 		file_ << '\n';
 	}
}

void OutputLM::BeginLength(unsigned int length) {
  fast_counter_ = 0;
  file_ << '\\' << length << "-grams:" << '\n';
}

void OutputLM::EndLength(unsigned int length) {
  file_ << '\n';
  if (length > counts_.size()) {
    counts_.resize(length);
  }
  counts_[length - 1] = fast_counter_;
}

void OutputLM::Finish() {
  file_ << "\\end\\\n";

  file_.seekp(0);
  file_ << "\n\\data\\\n";
  for (unsigned int i = 0; i < counts_.size(); ++i) {
    file_ << "ngram " << i+1 << "=" << counts_[i] << '\n';
  }
  file_ << '\n';
  if (max_count_space_ < file_.tellp()) {
    errx(1, "Oops messed up padding somehow.  This shouldn't happen.");
  }
  file_ << std::flush;
}

void ReadData(std::istream &in, std::vector<size_t> &number) {
	number.clear();
	std::string line;
	if (!getline(in, line)) err(2, "Reading input lm");
	if (!line.empty()) errx(3, "First line was \"%s\", not blank.", line.c_str());
	if (!getline(in, line)) err(2, "Reading \\data\\");
	if (!(line == "\\data\\")) err(3, "Second line was \"%s\", not blank.", line.c_str());
	while (getline(in, line)) {
		if (line.empty()) return;
		if (strncmp(line.c_str(), "ngram ", 6))
			errx(3, "data line \"%s\" doesn't begin with \"ngram \"", line.c_str());
		size_t equals = line.find('=');
		if (equals == std::string::npos)
			errx(3, "no equals in \"%s\".", line.c_str());
		unsigned int length = boost::lexical_cast<unsigned int>(line.substr(6, equals - 6));
		if (length - 1 != number.size()) errx(3, "ngram length %i is not expected %i in line %s", length, static_cast<unsigned int>(number.size() + 1), line.c_str());
		unsigned int count = boost::lexical_cast<unsigned int>(line.substr(equals + 1));
		number.push_back(count);		
	}
	err(2, "Reading input lm");
}

void ReadNGramHeader(std::istream &in, unsigned int length) {
  std::string line;
  do {
    if (!getline(in, line)) err(2, "Reading from input lm");
  } while (line.empty());
  if (line != (std::string("\\") + boost::lexical_cast<std::string>(length) + "-grams:"))
    errx(3, "Wrong ngram line: %s", line.c_str());
}

void ReadEnd(std::istream &in_lm) {
	std::string line;
	if (!getline(in_lm, line)) err(2, "Reading from input lm");
	if (line != "\\end\\") errx(3, "Bad end \"%s\"", line.c_str());
}

} // namespace lm
