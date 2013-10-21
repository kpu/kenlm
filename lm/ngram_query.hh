#ifndef LM_NGRAM_QUERY__
#define LM_NGRAM_QUERY__

#include "lm/enumerate_vocab.hh"
#include "lm/model.hh"
#include "util/usage.hh"

#include <cstdlib>
#include <iostream>
#include <ostream>
#include <istream>
#include <string>

#include <math.h>

namespace lm {
namespace ngram {

template <class Model> void Query(const Model &model, bool sentence_context, std::istream &in_stream, std::ostream &out_stream) {
  typename Model::State state, out;
  lm::FullScoreReturn ret;
  std::string word;

  double corpus_total = 0.0;
  uint64_t corpus_oov = 0;
  uint64_t corpus_tokens = 0;

  while (in_stream) {
    state = sentence_context ? model.BeginSentenceState() : model.NullContextState();
    float total = 0.0;
    bool got = false;
    uint64_t oov = 0;
    while (in_stream >> word) {
      got = true;
      lm::WordIndex vocab = model.GetVocabulary().Index(word);
      if (vocab == 0) ++oov;
      ret = model.FullScore(state, vocab, out);
      total += ret.prob;
      out_stream << word << '=' << vocab << ' ' << static_cast<unsigned int>(ret.ngram_length)  << ' ' << ret.prob << '\t';
      ++corpus_tokens;
      state = out;
      char c;
      while (true) {
        c = in_stream.get();
        if (!in_stream) break;
        if (c == '\n') break;
        if (!isspace(c)) {
          in_stream.unget();
          break;
        }
      }
      if (c == '\n') break;
    }
    if (!got && !in_stream) break;
    if (sentence_context) {
      ret = model.FullScore(state, model.GetVocabulary().EndSentence(), out);
      total += ret.prob;
      ++corpus_tokens;
      out_stream << "</s>=" << model.GetVocabulary().EndSentence() << ' ' << static_cast<unsigned int>(ret.ngram_length)  << ' ' << ret.prob << '\t';
    }
    out_stream << "Total: " << total << " OOV: " << oov << '\n';
    corpus_total += total;
    corpus_oov += oov;
  }
  out_stream << "Perplexity " << pow(10.0, -(corpus_total / static_cast<double>(corpus_tokens))) << std::endl;
}

template <class M> void Query(const char *file, bool sentence_context, std::istream &in_stream, std::ostream &out_stream) {
  Config config;
  M model(file, config);
  Query(model, sentence_context, in_stream, out_stream);
}

} // namespace ngram
} // namespace lm

#endif // LM_NGRAM_QUERY__


