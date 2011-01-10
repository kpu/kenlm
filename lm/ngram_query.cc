#include "lm/enumerate_vocab.hh"
#include "lm/model.hh"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

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

template <class Model> void Query(const Model &model) {
  PrintUsage("Loading statistics:\n");
  typename Model::State state, out;
  lm::FullScoreReturn ret;
  std::string word;

  while (std::cin) {
    state = model.BeginSentenceState();
    float total = 0.0;
    bool got = false;
    while (std::cin >> word) {
      got = true;
      lm::WordIndex vocab = model.GetVocabulary().Index(word);
      ret = model.FullScore(state, vocab, out);
      total += ret.prob;
      std::cout << word << '=' << vocab << ' ' << static_cast<unsigned int>(ret.ngram_length)  << ' ' << ret.prob << '\n';
      state = out;
      if (std::cin.get() == '\n') break;
    }
    if (!got && !std::cin) break;
    ret = model.FullScore(state, model.GetVocabulary().EndSentence(), out);
    total += ret.prob;
    std::cout << "</s>=" << model.GetVocabulary().EndSentence() << ' ' << static_cast<unsigned int>(ret.ngram_length)  << ' ' << ret.prob << '\n';
    std::cout << "Total: " << total << '\n';
  }
  PrintUsage("After queries:\n");
}

class PrintVocab : public lm::ngram::EnumerateVocab {
  public:
    void Add(lm::WordIndex index, const StringPiece &str) {
      std::cerr << "vocab " << index << ' ' << str << '\n';
    }
};

template <class Model> void Query(const char *name) {
  lm::ngram::Config config;
  PrintVocab printer;
  config.enumerate_vocab = &printer;
  Model model(name, config);
  Query(model);
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Pass language model name." << std::endl;
    return 0;
  }
  lm::ngram::ModelType model_type;
  if (lm::ngram::RecognizeBinary(argv[1], model_type)) {
    switch(model_type) {
      case lm::ngram::HASH_PROBING:
        Query<lm::ngram::ProbingModel>(argv[1]);
        break;
      case lm::ngram::HASH_SORTED:
        Query<lm::ngram::SortedModel>(argv[1]);
        break;
      case lm::ngram::TRIE_SORTED:
        Query<lm::ngram::TrieModel>(argv[1]);
        break;
      default:
        std::cerr << "Unrecognized kenlm model type " << model_type << std::endl;
        abort();
    }
  } else {
    Query<lm::ngram::ProbingModel>(argv[1]);
  }

  PrintUsage("Total time including destruction:\n");
}
