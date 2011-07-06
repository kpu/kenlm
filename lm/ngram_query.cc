#include "lm/enumerate_vocab.hh"
#include "lm/model.hh"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

#include <ctype.h>

#include <sys/resource.h>
#include <sys/time.h>

float FloatSec(const struct timeval &tv) {
  return static_cast<float>(tv.tv_sec) + (static_cast<float>(tv.tv_usec) / 1000000000.0);
}

void PrintUsage(const char *message) {
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
}

template <class Model> void Query(const Model &model, bool sentence_context) {
  PrintUsage("Loading statistics:\n");
  typename Model::State state, out;
  lm::FullScoreReturn ret;
  std::string word;

  while (std::cin) {
    state = sentence_context ? model.BeginSentenceState() : model.NullContextState();
    float total = 0.0;
    bool got = false;
    unsigned int oov = 0;
    while (std::cin >> word) {
      got = true;
      lm::WordIndex vocab = model.GetVocabulary().Index(word);
      if (vocab == 0) ++oov;
      ret = model.FullScore(state, vocab, out);
      total += ret.prob;
      std::cout << word << '=' << vocab << ' ' << static_cast<unsigned int>(ret.ngram_length)  << ' ' << ret.prob << '\t';
      state = out;
      char c;
      while (true) {
        c = std::cin.get();
        if (!std::cin) break;
        if (c == '\n') break;
        if (!isspace(c)) {
          std::cin.unget();
          break;
        }
      }
      if (c == '\n') break;
    }
    if (!got && !std::cin) break;
    if (sentence_context) {
      ret = model.FullScore(state, model.GetVocabulary().EndSentence(), out);
      total += ret.prob;
      std::cout << "</s>=" << model.GetVocabulary().EndSentence() << ' ' << static_cast<unsigned int>(ret.ngram_length)  << ' ' << ret.prob << '\t';
    }
    std::cout << "Total: " << total << " OOV: " << oov << '\n';
  }
  PrintUsage("After queries:\n");
}

template <class Model> void Query(const char *name) {
  lm::ngram::Config config;
  Model model(name, config);
  Query(model);
}

int main(int argc, char *argv[]) {
  if (!(argc == 2 || (argc == 3 && !strcmp(argv[2], "null")))) {
    std::cerr << "Usage: " << argv[0] << " lm_file [null]" << std::endl;
    std::cerr << "Input is wrapped in <s> and </s> unless null is passed." << std::endl;
    return 1;
  }
  bool sentence_context = (argc == 2);
  lm::ngram::ModelType model_type;
  if (lm::ngram::RecognizeBinary(argv[1], model_type)) {
    switch(model_type) {
      case lm::ngram::HASH_PROBING:
        Query<lm::ngram::ProbingModel>(argv[1], sentence_context);
        break;
      case lm::ngram::TRIE_SORTED:
        Query<lm::ngram::TrieModel>(argv[1], sentence_context);
        break;
      case lm::ngram::QUANT_TRIE_SORTED:
        Query<lm::ngram::QuantTrieModel>(argv[1], sentence_context);
        break;
      case lm::ngram::ARRAY_TRIE_SORTED:
        Query<lm::ngram::ArrayTrieModel>(argv[1], sentence_context);
        break;
      case lm::ngram::QUANT_ARRAY_TRIE_SORTED:
        Query<lm::ngram::QuantArrayTrieModel>(argv[1], sentence_context);
        break;
      case lm::ngram::HASH_SORTED:
      default:
        std::cerr << "Unrecognized kenlm model type " << model_type << std::endl;
        abort();
    }
  } else {
    Query<lm::ngram::ProbingModel>(argv[1], sentence_context);
  }

  PrintUsage("Total time including destruction:\n");
  return 0;
}
