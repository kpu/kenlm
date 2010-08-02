#include "lm/filter.hh"
#include "lm/phrase_substrings.hh"

#include <boost/function_output_iterator.hpp>

#include <algorithm>
#include <set>

namespace lm {

namespace {
// Optimized for sorted vector.
inline void VectorToSet(const std::vector<unsigned int> &vec, std::set<unsigned int> &out) {
  for (std::vector<unsigned int>::const_iterator i = vec.begin(); i != vec.end(); ++i)
    out.insert(out.end(), *i);
}

// Return iterator in first so that [first.begin(), return) agrees with second.  
std::vector<size_t>::const_iterator AgreeRange(const std::vector<size_t> &first, const std::vector<size_t> &second) {
  std::vector<size_t>::const_iterator f(first.begin()), s(second.begin());
  for (; f != first.end() && s != second.end() && (*f == *s); ++f, ++s) {}
  return f;
}

/* inserter from STL sends a position which makes the set insert slower.  This does random insert. */
class SetInsertAnywhere {
  public:
    explicit SetInsertAnywhere(std::set<unsigned int> &to) : set_(to) {}

    void operator()(unsigned int value) { set_.insert(value); }
  private:
    std::set<unsigned int> &set_;
};

boost::function_output_iterator<SetInsertAnywhere> FasterInserter(std::set<unsigned int> &to) {
  return boost::function_output_iterator<SetInsertAnywhere>(SetInsertAnywhere(to));
}

} // namespace

// Precondition: !hashes.empty()
template <bool EarlyExit> bool PhraseBinary::Evaluate() {
  assert(!hashes_.empty());
  // How much can we keep from the previous n-gram?  
  const std::vector<size_t>::const_iterator hash_agree = AgreeRange(hashes_, pre_hashes_);
  const size_t agree = hash_agree - hashes_.begin();
  if (hash_agree == hashes_.end()) {
    if (hashes_.size() != pre_hashes_.size()) {
      // Shorter than previous one.  
      swap(matches_, reach_[agree]);
      reach_.resize(agree - 1);
    }
    return !matches_.empty();
  }
  // reach_[i] is the set of sentences that reach _after_ word i.  There isn't a reach_[hashes.size() - 1] as this is called matches_.
  matches_.clear();
  reach_.resize(hashes_.size() - 1);
  for (std::vector<std::set<unsigned int> >::iterator i = reach_.begin() + agree; i != reach_.end(); ++i)
    i->clear();

  const std::vector<size_t>::const_iterator last_word = hashes_.end() - 1;

  size_t hash;
  std::vector<std::set<unsigned int> >::iterator reach_write;
  std::vector<size_t>::const_iterator hash_finish;

  const std::vector<unsigned int> *term;

  // Partial phrases off the beginning.  
  hash = 0;
  for (hash_finish = hashes_.begin(); hash_finish != hash_agree; ++hash_finish) {
    boost::hash_combine(hash, *hash_finish);
  }
  for (reach_write = reach_.begin() + agree; ; ++reach_write, ++hash_finish) {
    boost::hash_combine(hash, *hash_finish);
    // n-gram is a substring of a phrase: special case of off beginning that is also off end.  
    if (hash_finish == last_word) {
      // if (term) then we know !term->empty() because FindSubstring is the weakest criterion.   
      if ((term = substrings_.FindSubstring(hash))) {
//        if (EarlyExit) return true;
        VectorToSet(*term, matches_);
      }
      break;
    }

    if (!(term = substrings_.FindRight(hash))) break;
    VectorToSet(*term, *reach_write);
  }

  std::vector<size_t>::const_iterator hash_start;
  std::vector<std::set<unsigned int> >::const_iterator reach_intersect;
  // Loop over starting positions for phrases except the beginning which was already covered.  
  for (hash_start = hashes_.begin() + 1, reach_intersect = reach_.begin();
      hash_start != hashes_.end();
      ++hash_start, ++reach_intersect) {
    hash = 0;
    for (hash_finish = hash_start; hash_finish < hash_agree; ++hash_finish) {
      boost::hash_combine(hash, *hash_finish);
    }

    // Look over finishing positions for phrases.  
    for (reach_write = reach_.begin() + (hash_finish - hashes_.begin()); ; ++hash_finish, ++reach_write) {
      boost::hash_combine(hash, *hash_finish);
      // Allow phrases to go off the end of the n-gram.  
      if (hash_finish == last_word) {
        if ((term = substrings_.FindLeft(hash))) {
          set_intersection(reach_intersect->begin(), reach_intersect->end(), term->begin(), term->end(), FasterInserter(matches_));
//          if (EarlyExit && !matches.empty()) return true;
        }
        break;
      }
      if (!(term = substrings_.FindPhrase(hash))) break;
      set_intersection(reach_intersect->begin(), reach_intersect->end(), term->begin(), term->end(), FasterInserter(*reach_write));
    }
  }

  return !matches_.empty();
}

template bool PhraseBinary::Evaluate<true>();
template bool PhraseBinary::Evaluate<false>();

} // namespace lm
