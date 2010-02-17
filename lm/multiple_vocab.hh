#ifndef LM_MULTIPLE_VOCAB_H__
#define LM_MULTIPLE_VOCAB_H__

/* This is part of the LM filter.  It is the data structure for multiple
 * vocabularies.  */

#include "util/string_piece.hh"

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/unordered_map.hpp>

#include <istream>
#include <memory>
#include <string>
#include <vector>

namespace lm {

class PrepareMultipleVocab : boost::noncopyable {
  public:
    typedef boost::unordered_map<StringPiece, std::vector<unsigned int> > Vocabs;

    PrepareMultipleVocab() : temp_str_(new std::string) {
      to_insert_.second.push_back(0);
    }

    std::string &TempStr() {
      return *temp_str_;
    }

    void Insert() {
      to_insert_.first = StringPiece(*temp_str_);
      std::pair<Vocabs::iterator,bool> table(vocabs_.insert(to_insert_));
      if (table.second) {
        storage_.push_back(temp_str_);
        temp_str_.reset(new std::string());
      } else {
        if (table.first->second.back() != to_insert_.second.front()) {
          table.first->second.push_back(to_insert_.second.front());
        }
      }
    }

    void EndSentence() {
      ++MutableSentenceIndex();
    }

    // The PrepareMultipleVocab must still exist while this is used.
    const Vocabs &GetVocabs() const {
      return vocabs_;
    }

    unsigned int SentenceCount() const {
      return to_insert_.second.front();
    }

  private:
    unsigned int &MutableSentenceIndex() {
      return to_insert_.second.front();
    }

    boost::ptr_vector<std::string> storage_;
    Vocabs vocabs_;
    std::pair<StringPiece, std::vector<unsigned int> > to_insert_;
    std::auto_ptr<std::string> temp_str_;
};

// Read space separated words in enter separated lines.  
void ReadMultipleVocab(std::istream &in, PrepareMultipleVocab &out);

} // namespace lm

#endif // LM_MULTIPLE_VOCAB_H__
