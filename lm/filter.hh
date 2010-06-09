#ifndef LM_FILTER_H__
#define LM_FILTER_H__
/* Filter an ARPA language model to only contain words found in a vocabulary
 * plus tags with < and > around them.  
 */

#include "util/multi_intersection.hh"
#include "util/string_piece.hh"
#include "util/tokenize_piece.hh"

#include <boost/noncopyable.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>

#include <string>
#include <vector>

namespace lm {

/* Is this a special tag like <s> or <UNK>?  This actually includes anything
 * surrounded with < and >, which most tokenizers separate for real words, so
 * this should not catch real words as it looks at a single token.   
 */
inline bool IsTag(const StringPiece &value) {
  // The parser should never give an empty string.
  assert(!value.empty());
  return (value.data()[0] == '<' && value.data()[value.size() - 1] == '>');
}

class SingleBinary {
  public:
    typedef boost::unordered_set<std::string> Words;

    explicit SingleBinary(const Words &vocab) : vocab_(vocab) {}

    template <class Iterator> bool PassNGram(const Iterator &begin, const Iterator &end) {
      for (Iterator i = begin; i != end; ++i) {
        if (IsTag(*i)) continue;
        if (FindStringPiece(vocab_, *i) == vocab_.end()) return false;
      }
      return true;
    }

  private:
    const Words &vocab_;
};

class UnionBinary {
  public:
    typedef boost::unordered_map<std::string, std::vector<unsigned int> > Words;

    explicit UnionBinary(const Words &vocabs) : vocabs_(vocabs) {}

    template <class Iterator> bool PassNGram(const Iterator &begin, const Iterator &end) {
      sets_.clear();

      for (Iterator i(begin); i != end; ++i) {
        if (IsTag(*i)) continue;
        Words::const_iterator found(FindStringPiece(vocabs_, *i));
        if (vocabs_.end() == found) return false;
        sets_.push_back(boost::iterator_range<const unsigned int*>(&*found->second.begin(), &*found->second.end()));
      }
      return (sets_.empty() || util::FirstIntersection(sets_));
    }

  private:
    const Words &vocabs_;

    std::vector<boost::iterator_range<const unsigned int*> > sets_;
};

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

template <class OutputT> class MultipleOutputFilter {
  public:
    typedef OutputT Output;
    typedef boost::unordered_map<std::string, std::vector<unsigned int> > Words;

    MultipleOutputFilter(const Words &vocabs, Output &output) : vocabs_(vocabs), output_(output) {}

    Output &GetOutput() { return output_; }

    // Callback from AllIntersection that does AddNGram.
    class Callback {
      public:
        Callback(Output &out, const std::string &line) : out_(out), line_(line) {}

        void operator()(unsigned int index) {
          out_.SingleAddNGram(index, line_);
        }

      private:
        Output &out_;
        const std::string &line_;
    };

    template <class Iterator> void AddNGram(const Iterator &begin, const Iterator &end, const std::string &line) {
      sets_.clear();
      for (Iterator i(begin); i != end; ++i) {
        if (IsTag(*i)) continue;
        Words::const_iterator found(FindStringPiece(vocabs_, *i));
        if (vocabs_.end() == found) return;
        sets_.push_back(boost::iterator_range<const unsigned int*>(&*found->second.begin(), &*found->second.end()));
      }
      if (sets_.empty()) {
        output_.AddNGram(line);
        return;
      }

      Callback cb(output_, line);
      util::AllIntersection(sets_, cb);
    }

  private:
    const Words &vocabs_;

    Output &output_;

    std::vector<boost::iterator_range<const unsigned int*> > sets_;
};

/* Wrap another filter to pay attention only to context words */
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

#endif // LM_FILTER_H__
