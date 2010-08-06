#ifndef LM_FILTER_WRAPPER_H__
#define LM_FILTER_WRAPPER_H__

#include "util/string_piece.hh"

#include <string>
#include <vector>

namespace lm {

// Provide a single-output filter with the same interface as a
// multiple-output filter so clients code against one interface.
template <class Binary, class OutputT> class SingleOutputFilter {
  public:
    typedef OutputT Output;

    // Binary modles are just references (and a set) and it makes the API cleaner to copy them.  
    SingleOutputFilter(Binary binary, Output &output) : binary_(binary), output_(output) {}

    Output &GetOutput() { return output_; }

    template <class Iterator> void AddNGram(const Iterator &begin, const Iterator &end, const std::string &line) {
      if (binary_.PassNGram(begin, end))
        output_.AddNGram(line);
    }

  private:
    Binary binary_;
    Output &output_;
};

// Wrap another filter to pay attention only to context words
template <class FilterT> class ContextFilter {
  public:
    typedef FilterT Filter;
    typedef typename Filter::Output Output;

    ContextFilter(Filter &backend) : backend_(backend) {}

    Output &GetOutput() { return backend_.GetOutput(); }

    template <class Iterator> void AddNGram(const Iterator &begin, const Iterator &end, const std::string &line) {
      assert(begin != end);
      // TODO: this copy could be avoided by a lookahead iterator.
      pieces_.clear();
      std::copy(begin, end, std::back_insert_iterator<std::vector<StringPiece> >(pieces_));
      backend_.AddNGram(pieces_.begin(), pieces_.end() - 1, line);
    }

  private:
    std::vector<StringPiece> pieces_;

    Filter &backend_;
};

} // namespace lm

#endif // LM_FILTER_WRAPPER_H__
