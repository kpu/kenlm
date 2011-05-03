#include "lm/model.hh"

#include <fstream>
#include <string>
#include <iostream>

#include <ctype.h>

#include <unistd.h>

void PrintFile(const char *name) {
  std::ifstream proc_stat(name);
  std::string line;
  while (getline(proc_stat, line)) {
    std::cerr << line << '\n';
  }
}

void PrintStatus() {
  PrintFile("/proc/self/status");
  PrintFile("/proc/self/stat");
  std::cerr << "Clocks is " << sysconf(_SC_CLK_TCK) << std::endl;
}

template <class Model> void Score(const char *name) {
  lm::ngram::Config config;
  config.load_method = util::READ;
  Model m(name, config);
  std::cerr << "Loaded:" << std::endl;
  PrintStatus();
  std::vector<lm::WordIndex> indices(4096);

  typename Model::State state[2];
  state[0] = m.BeginSentenceState();
  typename Model::State *in_state = &state[0], *out_state = &state[1];
  lm::WordIndex delimit = m.GetVocabulary().EndSentence();
  float total = 0.0;
  while (true) {
    const size_t amount = fread(&*indices.begin(), sizeof(lm::WordIndex), indices.size(), stdin);
    if (!amount) {
      if (feof(stdin)) break;
      UTIL_THROW(util::ErrnoException, "Reading stdin");
    }
    for (const lm::WordIndex *i = &*indices.begin(); i != &*indices.begin() + amount; ++i) {
      total += m.FullScore(*in_state, *i, *out_state).prob;
      std::swap(in_state, out_state);
      if (*i == delimit) *in_state = m.BeginSentenceState();
    }
  }
  std::cout << total << std::endl;
  std::cerr << "Scored:" << std::endl;
  PrintStatus();
}

int main(int argc, char *argv[]) {
  if (argc != 2) UTIL_THROW(util::Exception, "Provide lm file name on command line.");
  lm::ngram::ModelType model_type;
  if (lm::ngram::RecognizeBinary(argv[1], model_type)) {
    switch(model_type) {
      case lm::ngram::HASH_PROBING:
        Score<lm::ngram::ProbingModel>(argv[1]);
        break;
      case lm::ngram::TRIE_SORTED:
        Score<lm::ngram::TrieModel>(argv[1]);
        break;
      case lm::ngram::HASH_SORTED:
      default:
        std::cerr << "Unrecognized kenlm model type " << model_type << std::endl;
        abort();
    }
    std::cerr << "Destroyed:" << std::endl;
    PrintStatus();
  } else {
    std::cerr << "Convert to binary first." << std::endl;
    return 1;
  }
}
