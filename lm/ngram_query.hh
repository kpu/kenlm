#ifndef LM_NGRAM_QUERY__
#define LM_NGRAM_QUERY__

#include "lm/enumerate_vocab.hh"
#include "lm/model.hh"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include <ctype.h>
#if !defined(_WIN32) && !defined(_WIN64)
#include <sys/resource.h>
#include <sys/time.h>
#endif

namespace lm {
namespace ngram {

#if !defined(_WIN32) && !defined(_WIN64)
float FloatSec(const struct timeval &tv) {
  return static_cast<float>(tv.tv_sec) + (static_cast<float>(tv.tv_usec) / 1000000000.0);
}
#endif

void PrintUsage(const char *message) {
#if !defined(_WIN32) && !defined(_WIN64)
  struct rusage usage;
  if (getrusage(RUSAGE_SELF, &usage)) {
    perror("getrusage");
    return;
  }
  std::cerr << message;
  std::cerr << "user\t" << FloatSec(usage.ru_utime) << "\nsys\t" << FloatSec(usage.ru_stime) << '\n';

  // Linux doesn't set memory usage :-(.  
  std::ifstream status("/proc/self/status", std::ios::in);
  std::string line;
  while (getline(status, line)) {
    if (!strncmp(line.c_str(), "VmRSS:\t", 7)) {
      std::cerr << "rss " << (line.c_str() + 7) << '\n';
      break;
    }
  }
#endif
}

template <class Model> void Query(const Model &model, bool sentence_context, std::istream &in_stream, std::ostream &out_stream) {
  PrintUsage("Loading statistics:\n");
  typename Model::State state, out;
  lm::FullScoreReturn ret;
  std::string word;

  while (in_stream) {
    state = sentence_context ? model.BeginSentenceState() : model.NullContextState();
    float total = 0.0;
    bool got = false;
    unsigned int oov = 0;
    while (in_stream >> word) {
      got = true;
      lm::WordIndex vocab = model.GetVocabulary().Index(word);
      if (vocab == 0) ++oov;
      ret = model.FullScore(state, vocab, out);
      total += ret.prob;
      out_stream << word << '=' << vocab << ' ' << static_cast<unsigned int>(ret.ngram_length)  << ' ' << ret.prob << '\t';
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
      out_stream << "</s>=" << model.GetVocabulary().EndSentence() << ' ' << static_cast<unsigned int>(ret.ngram_length)  << ' ' << ret.prob << '\t';
    }
    out_stream << "Total: " << total << " OOV: " << oov << '\n';
 }
  PrintUsage("After queries:\n");
}

template <class M> void Query(const char *file, bool sentence_context, std::istream &in_stream, std::ostream &out_stream) {
  Config config;
//  config.load_method = util::LAZY;
  M model(file, config);
  Query(model, sentence_context, in_stream, out_stream);
}

} // namespace ngram
} // namespace lm

#endif // LM_NGRAM_QUERY__


