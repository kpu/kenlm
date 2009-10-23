#ifndef LM_FILTER_H
#define LM_FILTER_H
/* Filter an ARPA language model to only contain words found in a vocabulary
 * plus <s>, </s>, and <unk>.
 */


#include "util/null_intersection.hh"
#include "util/string_piece.hh"
#include "util/tokenize_piece.hh"

#include <boost/lexical_cast.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>

#include <iostream>
#include <istream>
#include <memory>
#include <string>
#include <vector>

#include <err.h>
#include <string.h>

namespace lm {

inline bool IsTag(const StringPiece &value) {
	// The parser should never give an empty string.
	assert(!value.empty());
	return (value.data()[0] == '<' && value.data()[value.size() - 1] == '>');
}

class SingleVocabFilter {
	public:
		explicit SingleVocabFilter(std::istream &in) {
			// Insert SRI's custom words.
			words_.insert("<s>");
			words_.insert("</s>");
			words_.insert("<unk>");

			std::auto_ptr<std::string> word(new std::string());

			while (in >> *word) {
				if (words_.insert(StringPiece(*word)).second) {
					backing_.push_back(word);
					word.reset(new std::string());
				}
			}
			if (!in.eof()) err(1, "Reading text from stdin");
		}

		template <class Iterator> bool Keep(unsigned int length, const Iterator &begin, const Iterator &end) const {
			for (Iterator i = begin; i != end; ++i) {
				if (words_.find(*i) == words_.end()) return false;
			}
			return true;
		}
	private:
		// Keep this order so backing_ is deleted after words_.
		boost::ptr_vector<std::string> backing_;

		boost::unordered_set<StringPiece> words_;
};

class MultipleVocabFilter {
	public:
		typedef boost::unordered_map<StringPiece, std::vector<unsigned int> > Map;

		explicit MultipleVocabFilter(Map &vocabs) : vocabs_(vocabs) {}

		template <class Iterator> bool Keep(unsigned int length, const Iterator &begin, const Iterator &end) const {
			std::vector<util::BeginEnd<const unsigned int*> > sets;
			sets.reserve(length);

			Map::const_iterator found;
			for (Iterator i(begin); i != end; ++i) {
				if (IsTag(*i)) continue;
				if (vocabs_.end() == (found = vocabs_.find(*i))) return false;
				sets.push_back(util::BeginEnd<const unsigned int*>(&*found->second.begin(), &*found->second.end()));
			}
			return !util::NullIntersection(sets, std::less<unsigned int>());
		}

	private:
		const Map &vocabs_;
};

class OutputLM {
  public:
    OutputLM(std::ostream &file, std::streampos max_count_space);

    void BeginLength(unsigned int length);

    inline void AddNGram(const std::string &line) {
      file_ << line << '\n';
      ++fast_counter_;
    }

    void EndLength(unsigned int length);

    void Finish();

  private:
    std::ostream &file_;
    size_t fast_counter_;
    std::streampos max_count_space_;
    std::vector<size_t> counts_;
};

template <class Filter> void FilterNGrams(std::istream &in, unsigned int l, size_t number, const Filter &filter, OutputLM &out) {
	std::string line;
	if (!getline(in, line)) err(2, "Reading from input lm");
	if (line != (std::string("\\") + boost::lexical_cast<std::string>(l) + "-grams:"))
		errx(3, "Wrong ngram line: %s", line.c_str());
  out.BeginLength(l);
	for (unsigned int i = 0; i < number; ++i) {
		if (!(i % 100000)) {
			std::cerr << "Length " << l << ": " << i << "/" << number << std::endl;
		}
		if (!std::getline(in, line))
			err(2, "Reading ngram failed.  Maybe the counts are wrong?");

		util::PieceIterator<'\t'> tabber(line);
		if (!tabber) 
			errx(3, "Empty \"%s\"", line.c_str());
		if (!++tabber)
			errx(3, "No tab in line \"%s\"", line.c_str());

		if (filter.Keep(l, util::PieceIterator<' '>(*tabber), util::PieceIterator<' '>::end())) {
			out.AddNGram(line);
		}
	}
	if (!getline(in, line)) err(2, "Reading from input lm");
	if (!line.empty()) errx(3, "Expected blank line after ngrams");
	out.EndLength(l);
}

void ReadData(std::istream &in, std::vector<size_t> &number);
void ReadEnd(std::istream &in_lm);

template <class Filter> void FilterARPA(const Filter &to, std::istream &in_lm, std::ostream &out_file) {
	std::vector<size_t> number;
	ReadData(in_lm, number);
  OutputLM out(out_file, in_lm.tellg());
	for (unsigned int i = 0; i < number.size(); ++i) {
		FilterNGrams(in_lm, i + 1, number[i], to, out);
	}
	ReadEnd(in_lm);

  out.Finish();
}

} // namespace lm

#endif // LM_FILTER_H
