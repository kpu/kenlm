#include "lm/ngram_query.hh"
#include "util/getopt.hh"

#ifdef WITH_NPLM
#include "lm/wrappers/nplm.hh"
#endif

#include <stdlib.h>

void Usage(const char *name) {
  std::cerr <<
    "KenLM was compiled with maximum order " << KENLM_MAX_ORDER << ".\n"
    "Usage: " << name << " [-n] [-s] lm_file\n"
    "-n: Do not wrap the input in <s> and </s>.\n"
    "-s: Sentence totals only.\n"
    "-l lazy|populate|read|parallel: Load lazily, with populate, or malloc+read\n"
    "The default loading method is populate on Linux and read on others.\n";
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc == 1 || (argc == 2 && !strcmp(argv[1], "--help")))
    Usage(argv[0]);

  lm::ngram::Config config;
  bool sentence_context = true;
  bool show_words = true;

  int opt;
  while ((opt = getopt(argc, argv, "hnsl:")) != -1) {
    switch (opt) {
      case 'n':
        sentence_context = false;
        break;
      case 's':
        show_words = false;
        break;
      case 'l':
        if (!strcmp(optarg, "lazy")) {
          config.load_method = util::LAZY;
        } else if (!strcmp(optarg, "populate")) {
          config.load_method = util::POPULATE_OR_READ;
        } else if (!strcmp(optarg, "read")) {
          config.load_method = util::READ;
        } else if (!strcmp(optarg, "parallel")) {
          config.load_method = util::PARALLEL_READ;
        } else {
          Usage(argv[0]);
        }
        break;
      case 'h':
      default:
        Usage(argv[0]);
    }
  }
  if (optind + 1 != argc)
    Usage(argv[0]);
  const char *file = argv[optind];
  try {
    using namespace lm::ngram;
    ModelType model_type;
    if (RecognizeBinary(file, model_type)) {
      switch(model_type) {
        case PROBING:
          Query<lm::ngram::ProbingModel>(file, config, sentence_context, show_words);
          break;
        case REST_PROBING:
          Query<lm::ngram::RestProbingModel>(file, config, sentence_context, show_words);
          break;
        case TRIE:
          Query<TrieModel>(file, config, sentence_context, show_words);
          break;
        case QUANT_TRIE:
          Query<QuantTrieModel>(file, config, sentence_context, show_words);
          break;
        case ARRAY_TRIE:
          Query<ArrayTrieModel>(file, config, sentence_context, show_words);
          break;
        case QUANT_ARRAY_TRIE:
          Query<QuantArrayTrieModel>(file, config, sentence_context, show_words);
          break;
        default:
          std::cerr << "Unrecognized kenlm model type " << model_type << std::endl;
          abort();
      }
#ifdef WITH_NPLM
    } else if (lm::np::Model::Recognize(file)) {
      lm::np::Model model(file);
      if (show_words) {
        Query<lm::np::Model, lm::ngram::FullPrint>(model, sentence_context);
      } else {
        Query<lm::np::Model, lm::ngram::BasicPrint>(model, sentence_context);
      }
#endif
    } else {
      Query<ProbingModel>(file, config, sentence_context, show_words);
    }
    util::PrintUsage(std::cerr);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
