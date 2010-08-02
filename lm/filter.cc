#include "lm/filter.hh"
#include "lm/phrase_substrings.hh"

#include <algorithm>
#include <set>

namespace lm {

namespace {
// Optimized for sorted vector.
void VectorToSet(const std::vector<unsigned int> &vec, std::set<unsigned int> &out) {
  for (std::vector<unsigned int>::const_iterator i = vec.begin(); i != vec.end(); ++i)
    out.insert(out.end(), *i);
}
}

bool PhraseBinary::Evaluate() const {
  if (hashes_.empty()) return true;
  // reach[i] is the set of sentences that reach _after_ word i.  
  std::vector<std::set<unsigned int> > reach(hashes_.size());

  const std::vector<size_t>::const_iterator last_word = hashes_.end() - 1;

  size_t hash;
  std::vector<std::set<unsigned int> >::iterator reach_write;
  std::vector<size_t>::const_iterator hash_finish;

  // Partial phrases off the beginning.  
  hash = 0;
  for (reach_write = reach.begin(), hash_finish = hashes_.begin(); hash_finish < last_word; ++reach_write, ++hash_finish) {
    boost::hash_combine(hash, *hash_finish);
    const std::vector<unsigned int> *term = substrings_.FindRight(hash);
    if (!term) break;
    VectorToSet(*term, *reach_write);
  }
  // n-gram is a substring of a phrase: special case of off beginning that is also off end.  
  if (hash_finish == last_word) {
    boost::hash_combine(hash, *hash_finish);
    const std::vector<unsigned int> *term = substrings_.FindSubstring(hash);
    // if (term) then we know !term->empty() because FindSubstring is the weakest criterion.   
    if (term) return true; //VectorToSet(*term, *reach_write);
  }

  // All starting points except the beginning.  
  std::vector<size_t>::const_iterator hash_start;
  std::vector<std::set<unsigned int> >::const_iterator reach_intersect;
  std::vector<std::set<unsigned int> >::iterator reach_write_begin;
  for (hash_start = hashes_.begin() + 1, reach_intersect = reach.begin(), reach_write_begin = reach.begin() + 1; 
      hash_start != hashes_.end();
      ++hash_start, ++reach_intersect, ++reach_write_begin) {
    hash = 0;
    for (hash_finish = hash_start, reach_write = reach_write_begin; hash_finish < last_word; ++hash_finish, ++reach_write) {
      boost::hash_combine(hash, *hash_finish);
      const std::vector<unsigned int> *term = substrings_.FindPhrase(hash);
      if (!term) break;
      set_intersection(reach_intersect->begin(), reach_intersect->end(), term->begin(), term->end(), inserter(*reach_write, reach_write->end()));
    }
    // Allow phrases to go off the end of the n-gram.  
    if (hash_finish == last_word) {
      boost::hash_combine(hash, *hash_finish);
      const std::vector<unsigned int> *term = substrings_.FindLeft(hash);
      if (term) {
        set_intersection(reach_intersect->begin(), reach_intersect->end(), term->begin(), term->end(), inserter(*reach_write, reach_write->end()));
        if (!reach_write->empty()) return true;
      }
    }
  }

  return false;
}

} // namespace lm
