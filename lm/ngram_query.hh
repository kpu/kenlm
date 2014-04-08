#ifndef LM_NGRAM_QUERY_H
#define LM_NGRAM_QUERY_H

#include "lm/enumerate_vocab.hh"
#include "lm/model.hh"
#include "util/file_piece.hh"
#include "util/usage.hh"

#include <cstdlib>
#include <iostream>
#include <ostream>
#include <istream>
#include <string>

#include <math.h>

namespace lm {
namespace ngram {

template <class Model> void Query(const Model &model, bool sentence_context) {
  typename Model::State state, out;
  lm::FullScoreReturn ret;
  StringPiece word;

  util::FilePiece in(0);
  std::ostream &out_stream = std::cout;

  double corpus_total = 0.0;
  double corpus_total_oov_only = 0.0;
  uint64_t corpus_oov = 0;
  uint64_t corpus_tokens = 0;

  while (true) {
    state = sentence_context ? model.BeginSentenceState() : model.NullContextState();
    float total = 0.0;
    uint64_t oov = 0;

    while (in.ReadWordSameLine(word)) {
      lm::WordIndex vocab = model.GetVocabulary().Index(word);
      ret = model.FullScore(state, vocab, out);
      if (vocab == model.GetVocabulary().NotFound()) {
        ++oov;
        corpus_total_oov_only += ret.prob;
      }
      total += ret.prob;
      out_stream << word << '=' << vocab << ' ' << static_cast<unsigned int>(ret.ngram_length)  << ' ' << ret.prob << '\t';
      ++corpus_tokens;
      state = out;
    }
    // If people don't have a newline after their last query, this won't add a </s>.
    // Sue me.
    try {
      UTIL_THROW_IF('\n' != in.get(), util::Exception, "FilePiece is confused.");
    } catch (const util::EndOfFileException &e) { break; }
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
  out_stream << 
    "Perplexity including OOVs:\t" << pow(10.0, -(corpus_total / static_cast<double>(corpus_tokens))) << "\n"
    "Perplexity excluding OOVs:\t" << pow(10.0, -((corpus_total - corpus_total_oov_only) / static_cast<double>(corpus_tokens - corpus_oov))) << "\n"
    "OOVs:\t" << corpus_oov << "\n"
    ;
}

template <class M> void Query(const char *file, bool sentence_context) {
  Config config;
  M model(file, config);
  Query(model, sentence_context);
}

} // namespace ngram
} // namespace lm

#endif // LM_NGRAM_QUERY_H


