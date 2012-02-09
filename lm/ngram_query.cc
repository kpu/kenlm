#include "lm/ngram_query.hh"

int main(int argc, char *argv[]) {
  if (!(argc == 2 || (argc == 3 && !strcmp(argv[2], "null")))) {
    std::cerr << "Usage: " << argv[0] << " lm_file [null]" << std::endl;
    std::cerr << "Input is wrapped in <s> and </s> unless null is passed." << std::endl;
    return 1;
  }
  try {
    bool sentence_context = (argc == 2);
    using namespace lm::ngram;
    ModelType model_type;
    if (RecognizeBinary(argv[1], model_type)) {
      switch(model_type) {
        case HASH_PROBING:
          Query<lm::ngram::ProbingModel>(argv[1], sentence_context, std::cin, std::cout);
          break;
        case TRIE_SORTED:
          Query<TrieModel>(argv[1], sentence_context, std::cin, std::cout);
          break;
        case QUANT_TRIE_SORTED:
          Query<QuantTrieModel>(argv[1], sentence_context, std::cin, std::cout);
          break;
        case ARRAY_TRIE_SORTED:
          Query<ArrayTrieModel>(argv[1], sentence_context, std::cin, std::cout);
          break;
        case QUANT_ARRAY_TRIE_SORTED:
          Query<QuantArrayTrieModel>(argv[1], sentence_context, std::cin, std::cout);
          break;
        case HASH_SORTED:
        default:
          std::cerr << "Unrecognized kenlm model type " << model_type << std::endl;
          abort();
      }
    } else {
      Query<ProbingModel>(argv[1], sentence_context, std::cin, std::cout);
    }

    PrintUsage("Total time including destruction:\n");
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
