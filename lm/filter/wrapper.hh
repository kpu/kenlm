#ifndef LM_FILTER_WRAPPER_H__
#define LM_FILTER_WRAPPER_H__

#include "util/string_piece.hh"

#include <algorithm>
#include <string>
#include <vector>

namespace lm {

// Provide a single-output filter with the same interface as a
// multiple-output filter so clients code against one interface.
template <class Binary> class BinaryFilter {
  public:
    // Binary modes are just references (and a set) and it makes the API cleaner to copy them.  
    explicit BinaryFilter(Binary binary) : binary_(binary) {}

    template <class Iterator, class Output> void AddNGram(const Iterator &begin, const Iterator &end, const StringPiece &line, Output &output) {
      if (binary_.PassNGram(begin, end))
        output.AddNGram(line);
    }

    template <class Output> void AddNGram(const StringPiece &ngram, const StringPiece &line, Output &output) {
      AddNGram(util::TokenIter<util::SingleCharacter, true>(ngram, ' '), util::TokenIter<util::SingleCharacter, true>::end(), line, output);
    }

    void Flush() const {}

  private:
    Binary binary_;
};

// Wrap another filter to pay attention only to context words
template <class FilterT> class ContextFilter {
  public:
    typedef FilterT Filter;

    explicit ContextFilter(Filter &backend) : backend_(backend) {}

    template <class Output> void AddNGram(const StringPiece &ngram, const StringPiece &line, Output &output) {
      pieces_.clear();
      // TODO: this copy could be avoided by a lookahead iterator.
      std::copy(util::TokenIter<util::SingleCharacter, true>(ngram, ' '), util::TokenIter<util::SingleCharacter, true>::end(), std::back_insert_iterator<std::vector<StringPiece> >(pieces_));
      backend_.AddNGram(pieces_.begin(), pieces_.end() - !pieces_.empty(), line, output);
    }

    void Flush() const {}

  private:
    std::vector<StringPiece> pieces_;

    Filter backend_;
};

} // namespace lm

#endif // LM_FILTER_WRAPPER_H__
