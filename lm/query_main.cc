#include "lm/ngram_query.hh"

#ifdef WITH_NPLM
#include "lm/wrappers/nplm.hh"
#endif

#include <stdlib.h>

void Usage(const char *name) {
  std::cerr << "KenLM was compiled with maximum order " << KENLM_MAX_ORDER << "." << std::endl;
  std::cerr << "Usage: " << name << " [-n] lm_file" << std::endl;
  std::cerr << "Input is wrapped in <s> and </s> unless -n is passed." << std::endl;
  exit(1);
}

int main(int argc, char *argv[]) {
  bool sentence_context = true;
  const char *file = NULL;
  for (char **arg = argv + 1; arg != argv + argc; ++arg) {
    if (!strcmp(*arg, "-n")) {
      sentence_context = false;
    } else if (!strcmp(*arg, "-h") || !strcmp(*arg, "--help") || file) {
      Usage(argv[0]);
    } else {
      file = *arg;
    }
  }
  if (!file) Usage(argv[0]);
  try {
    using namespace lm::ngram;
    ModelType model_type;
    if (RecognizeBinary(file, model_type)) {
      switch(model_type) {
        case PROBING:
          Query<lm::ngram::ProbingModel>(file, sentence_context, std::cin, std::cout);
          break;
        case REST_PROBING:
          Query<lm::ngram::RestProbingModel>(file, sentence_context, std::cin, std::cout);
          break;
        case TRIE:
          Query<TrieModel>(file, sentence_context, std::cin, std::cout);
          break;
        case QUANT_TRIE:
          Query<QuantTrieModel>(file, sentence_context, std::cin, std::cout);
          break;
        case ARRAY_TRIE:
          Query<ArrayTrieModel>(file, sentence_context, std::cin, std::cout);
          break;
        case QUANT_ARRAY_TRIE:
          Query<QuantArrayTrieModel>(file, sentence_context, std::cin, std::cout);
          break;
        default:
          std::cerr << "Unrecognized kenlm model type " << model_type << std::endl;
          abort();
      }
#ifdef WITH_NPLM
    } else if (lm::np::Model::Recognize(file)) {
      lm::np::Model model(file);
      Query(model, sentence_context, std::cin, std::cout);
#endif
    } else {
      Query<ProbingModel>(file, sentence_context, std::cin, std::cout);
    }
    std::cerr << "Total time including destruction:\n";
    util::PrintUsage(std::cerr);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
