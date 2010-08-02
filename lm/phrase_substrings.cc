#include "lm/phrase_substrings.hh"

#include <boost/functional/hash.hpp>

#include <iostream>
#include <string>
#include <vector>

#include <ctype.h>

namespace lm {

unsigned int ReadMultiplePhrase(std::istream &in, PhraseSubstrings &out) {
  bool sentence_content = false;
  unsigned int sentence_id = 0;
  std::vector<size_t> phrase;
  std::string word;
  boost::hash<std::string> hasher;
  while (in) {
    char c;
    // Gather a word.
    while (!isspace(c = in.get()) && in) word += c;
    // Treat EOF like a newline.
    if (!in) c = '\n';
    // Add the word to the phrase.
    if (!word.empty()) {
      phrase.push_back(hasher(word));
      word.clear();
    }
    if (c == ' ') continue;
    // It's more than just a space.  Close out the phrase.  
    if (!phrase.empty()) {
      sentence_content = true;
      out.AddPhrase(sentence_id, phrase.begin(), phrase.end());
      phrase.clear();
    }
    if (c == '\t' || c == '\v') continue;
    // It's more than a space or tab: a newline.   
    if (sentence_content) {
      ++sentence_id;
      sentence_content = false;
    }
  }
  if (!in.eof()) in.exceptions(std::istream::failbit | std::istream::badbit);
  return sentence_id + sentence_content;
}

} // namespace lm
