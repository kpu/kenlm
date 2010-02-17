#ifndef LM_FILTER_H__
#define LM_FILTER_H__
/* Filter an ARPA language model to only contain words found in a vocabulary
 * plus <s>, </s>, and <unk>.
 */

#include "lm/arpa_io.hh"
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

/* Base class for filters that have a single output */
class SingleOutputFilter : boost::noncopyable {
  public:
    void ReserveForCounts(std::streampos reserve) {
      out_.ReserveForCounts(reserve);
    }

    void BeginLength(unsigned int length) {
      out_.BeginLength(length);
    }

    void EndLength(unsigned int length) {
      out_.EndLength(length);
    }

    void Finish() {
      out_.Finish();
    }

  protected:
    explicit SingleOutputFilter(const char *out) : out_(out) {}

    ARPAOutput out_;
};


class SingleVocabFilter : public SingleOutputFilter {
  public:
    typedef boost::unordered_set<StringPiece> Set;

    SingleVocabFilter(std::istream &vocab, const char *out);

    template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string &line) {
      for (Iterator i = begin; i != end; ++i) {
        if (IsTag(*i)) continue;
        if (words_.find(*i) == words_.end()) return;
      }
      out_.AddNGram(line);
    }

  private:
    // Keep this order so backing_ is deleted after words_.
    boost::ptr_vector<std::string> backing_;

    boost::unordered_set<StringPiece> words_;
};

class MultipleVocabSingleOutputFilter : public SingleOutputFilter {
  public:
    typedef boost::unordered_map<StringPiece, std::vector<unsigned int> > Map;

    MultipleVocabSingleOutputFilter(const Map &vocabs, const char *out) : SingleOutputFilter(out), vocabs_(vocabs) {}

    template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string &line) {
      std::vector<boost::iterator_range<const unsigned int*> > sets;
      sets.reserve(length);

      Map::const_iterator found;
      for (Iterator i(begin); i != end; ++i) {
        if (IsTag(*i)) continue;
        if (vocabs_.end() == (found = vocabs_.find(*i))) return;
        sets.push_back(boost::iterator_range<const unsigned int*>(&*found->second.begin(), &*found->second.end()));
      }
      if (sets.empty() || util::FirstIntersection(sets)) {
        out_.AddNGram(line);
      }
    }

  private:
    const Map &vocabs_;
};

class MultipleVocabMultipleOutputFilter {
  public:
    typedef boost::unordered_map<StringPiece, std::vector<unsigned int> > Map;

    MultipleVocabMultipleOutputFilter(const Map &vocabs, unsigned int sentence_count, const char *prefix);

    void ReserveForCounts(std::streampos reserve) {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i) {
        i->ReserveForCounts(reserve);
      }
    }

    void BeginLength(unsigned int length) {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i) {
        i->BeginLength(length);
      }
    }

    // Callback from AllIntersection that does AddNGram.
    class Callback {
      public:
        Callback(boost::ptr_vector<ARPAOutput> &files, const std::string &line) : files_(files), line_(line) {}

        void operator()(unsigned int index) {
          files_[index].AddNGram(line_);
        }

      private:
        boost::ptr_vector<ARPAOutput> &files_;
        const std::string &line_;
    };

    template <class Iterator> void AddNGram(unsigned int length, const Iterator &begin, const Iterator &end, const std::string &line) {
      std::vector<boost::iterator_range<const unsigned int*> > sets;
      sets.reserve(length);

      Map::const_iterator found;
      for (Iterator i(begin); i != end; ++i) {
        if (IsTag(*i)) continue;
        if (vocabs_.end() == (found = vocabs_.find(*i))) return;
        sets.push_back(boost::iterator_range<const unsigned int*>(&*found->second.begin(), &*found->second.end()));
      }
      if (sets.empty()) {
        for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i) {
          i->AddNGram(line);
        }
        return;
      }

      Callback cb(files_, line);
      util::AllIntersection(sets, cb);
    }

    void EndLength(unsigned int length) {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i) {
        i->EndLength(length);
      }
    }

    void Finish() {
      for (boost::ptr_vector<ARPAOutput>::iterator i = files_.begin(); i != files_.end(); ++i) {
        i->Finish();
      }
    }

  private:
    boost::ptr_vector<ARPAOutput> files_;
    const Map &vocabs_;
};

} // namespace lm

#endif // LM_FILTER_H__
