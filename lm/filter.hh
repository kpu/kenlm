#ifndef LM_FILTER_H__
#define LM_FILTER_H__
/* Filter an ARPA language model to only contain words found in a vocabulary
 * plus <s>, </s>, and <unk>.
 */

#include "util/multi_intersection.hh"
#include "util/string_piece.hh"
#include "util/tokenize_piece.hh"

#include <boost/lexical_cast.hpp>
#include <boost/noncopyable.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/range/iterator_range.hpp>
#include <boost/unordered/unordered_map.hpp>
#include <boost/unordered/unordered_set.hpp>

#include <istream>
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
    typedef boost::unordered_set<StringPiece> Set;

    explicit SingleBinary(std::istream &vocab);

    template <class Iterator> bool PassNGram(unsigned int length, const Iterator &begin, const Iterator &end) {
      for (Iterator i = begin; i != end; ++i) {
        if (IsTag(*i)) continue;
        if (words_.find(*i) == words_.end()) return false;
      }
      return true;
    }

  protected:
    // Keep this order so backing_ is deleted after words_.
    boost::ptr_vector<std::string> backing_;

    boost::unordered_set<StringPiece> words_;
};

class UnionBinary {
  public:
    typedef boost::unordered_map<StringPiece, std::vector<unsigned int> > Map;

    explicit UnionBinary(const Map &vocabs) : vocabs_(vocabs) {}

    template <class Iterator> bool PassNGram(unsigned int length, const Iterator &begin, const Iterator &end) {
      sets_.clear();
      Map::const_iterator found;

      for (Iterator i(begin); i != end; ++i) {
        if (IsTag(*i)) continue;
        if (vocabs_.end() == (found = vocabs_.find(*i))) return false;
        sets_.push_back(boost::iterator_range<const unsigned int*>(&*found->second.begin(), &*found->second.end()));
      }
      return (sets_.empty() || util::FirstIntersection(sets_));
    }

  private:
    const Map &vocabs_;

    std::vector<boost::iterator_range<const unsigned int*> > sets_;
};

template <class Binary, class OutputT> class SingleOutputFilter {
  public:
    typedef OutputT Output;

    SingleOutputFilter(Binary &binary, Output &output) : binary_(binary), output_(output) {}

    Output &GetOutput() { return output_; }

    template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string &line) {
      if (binary_.PassNGram(length, begin, end))
        output_.AddNGram(line);
    }

  private:
    Binary &binary_;
    Output &output_;
};

template <class OutputT> class MultipleOutputFilter {
  public:
    typedef OutputT Output;
    typedef boost::unordered_map<StringPiece, std::vector<unsigned int> > Map;

    MultipleOutputFilter(const Map &vocabs, Output &output) : vocabs_(vocabs), output_(output) {}

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

    template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string &line) {
      sets_.clear();
      Map::const_iterator found;
      for (Iterator i(begin); i != end; ++i) {
        if (IsTag(*i)) continue;
        if (vocabs_.end() == (found = vocabs_.find(*i))) return;
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
    const Map &vocabs_;

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

    template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string &line) {
      assert(length);
      // TODO: check this is more efficient than just parsing the string twice.  
      pieces_.clear();
      unsigned int i;
      Iterator it(begin);
      for (i = 0; i < length - 1; ++i, ++it) {
        pieces_.push_back(*it);
      }
      backend_.AddNGram(length - 1, pieces_.begin(), pieces_.end(), line);
    }

  private:
    std::vector<StringPiece> pieces_;

    Filter &backend_;
};

} // namespace lm

#endif // LM_FILTER_H__
