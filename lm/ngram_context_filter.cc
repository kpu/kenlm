#include "lm/multiple_vocab.hh"
#include "util/multi_intersection.hh"
#include "util/string_piece.hh"
#include "util/tokenize_piece.hh"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>

#include <err.h>

inline bool IsTag(const StringPiece &value) {
  // The parser should never give an empty string.
  assert(!value.empty());
  return (value.data()[0] == '<' && value.data()[value.size() - 1] == '>');
}

class Filter {
  public:
    explicit Filter(const boost::unordered_map<StringPiece, std::vector<unsigned int> > &vocabs) : vocabs_(vocabs) {
      sets_.reserve(6);
    }

    void AddNGram(const std::string &line, util::PieceIterator<' '> &words, const StringPiece &count) {
      sets_.clear();
      if (!words) errx(2, "Empty words");
      // This loop collects all but the last word.  
      while (true) {
        StringPiece current(*words);
        if (!++words) break;

        if (IsTag(current)) continue;

        boost::unordered_map<StringPiece, std::vector<unsigned int> >::const_iterator found(vocabs_.find(current));
        if (found == vocabs_.end()) return;
        sets_.push_back(boost::iterator_range<const unsigned int*>(&*found->second.begin(), &*found->second.end()));
      }
      if (sets_.empty() || util::FirstIntersection(sets_)) {
        std::cout << line << '\n';
      }
    }

  private:
    const boost::unordered_map<StringPiece, std::vector<unsigned int> > vocabs_;

    std::vector<boost::iterator_range<const unsigned int*> > sets_;
};

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Expected vocabulary file on command line." << std::endl;
  }
  lm::PrepareMultipleVocab prep;
  {
    std::ifstream vocab_in(argv[1], std::ios::in);
    ReadMultipleVocab(vocab_in, prep);
  }
  Filter filter(prep.GetVocabs());

  std::string line;
  while (getline(std::cin, line)) {
    util::PieceIterator<'\t'> tabber(line);
    if (!tabber) errx(2, "Bad line %s", line.c_str());
    util::PieceIterator<' '> words(*tabber);
    if (!++tabber) errx(2, "Missing counts in %s", line.c_str());
    filter.AddNGram(line, words, *tabber);
  }
  if (!std::cin.eof()) {
    err(1, "Reading stdin");
  }
}
