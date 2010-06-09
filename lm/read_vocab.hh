#ifndef LM_READ_VOCAB_H__
#define LM_READ_VOCAB_H__

/* This is part of the LM filter.  It is the default file formats for
 * vocabulary files. */

#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include <istream>
#include <string>
#include <vector>

namespace lm {

void ReadSingleVocab(std::istream &in, boost::unordered_set<std::string> &out);

// Read one sentence vocabulary per line.  Return the number of sentences.
unsigned int ReadMultipleVocab(std::istream &in, boost::unordered_map<std::string, std::vector<unsigned int> > &out);

} // namespace lm

#endif // LM_READ_VOCAB_H__
