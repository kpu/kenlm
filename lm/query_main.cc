#include "ngram_query.hh"
#include "../util/getopt.hh"

#ifdef WITH_NPLM
#include "wrappers/nplm.hh"
#endif

#include <stdlib.h>

void Usage(const char *name) {
  std::cerr <<
    "KenLM was compiled with maximum order " << KENLM_MAX_ORDER << ".\n"
    "Usage: " << name << " [-b] [-n] [-w] [-s] lm_file\n"
    "-b: Do not buffer output.\n"
    "-n: Do not wrap the input in <s> and </s>.\n"
    "-v summary|sentence|word: Print statistics at this level.\n"
    "   Can be used multiple times: -v summary -v sentence -v word\n"
    "-l lazy|populate|read|parallel: Load lazily, with populate, or malloc+read\n"
    "The default loading method is populate on Linux and read on others.\n\n"
    "Each word in the output is formatted as:\n"
    "  word=vocab_id ngram_length log10(p(word|context))\n"
    "where ngram_length is the length of n-gram matched.  A vocab_id of 0 indicates\n"
    "the unknown word. Sentence-level output includes log10 probability of the\n"
    "sentence and OOV count.\n";
  exit(1);
}

int main(int argc, char *argv[]) {
  if (argc == 1 || (argc == 2 && !strcmp(argv[1], "--help")))
    Usage(argv[0]);

  lm::ngram::Config config;
  bool sentence_context = true;
  bool print_word = false;
  bool print_line = false;
  bool print_summary = false;
  bool flush = false;

  int opt;
  while ((opt = getopt(argc, argv, "bnv:l:")) != -1) {
    switch (opt) {
      case 'b':
        flush = true;
        break;
      case 'n':
        sentence_context = false;
        break;
      case 'v':
        if (!strcmp(optarg, "2")) {
          print_word = true;
          print_line = true;
          print_summary = true;
        } else if (!strcmp(optarg, "1")) {
          print_word = false;
          print_line = true;
          print_summary = true;
        } else if (!strcmp(optarg, "0")) {
          print_word = false;
          print_line = false;
          print_summary = true;
        } else if (!strcmp(optarg, "word")) {
          print_word = true;
        } else if (!strcmp(optarg, "sentence")) {
          print_line = true;
        } else if (!strcmp(optarg, "summary")) {
          print_summary = true;
        } else {
          Usage(argv[0]);
        }
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
  // No verbosity argument specified.
  if (!print_word && !print_line && !print_summary) {
    print_word = true;
    print_line = true;
    print_summary = true;
  }
  lm::ngram::QueryPrinter printer(1, print_word, print_line, print_summary, flush);
  const char *file = argv[optind];
  try {
    using namespace lm::ngram;
    ModelType model_type;
    if (RecognizeBinary(file, model_type)) {
      std::cerr << "This binary file contains " << lm::ngram::kModelNames[model_type] << "." << std::endl;
      switch(model_type) {
        case PROBING:
          Query<lm::ngram::ProbingModel>(file, config, sentence_context, printer);
          break;
        case REST_PROBING:
          Query<lm::ngram::RestProbingModel>(file, config, sentence_context, printer);
          break;
        case TRIE:
          Query<TrieModel>(file, config, sentence_context, printer);
          break;
        case QUANT_TRIE:
          Query<QuantTrieModel>(file, config, sentence_context, printer);
          break;
        case ARRAY_TRIE:
          Query<ArrayTrieModel>(file, config, sentence_context, printer);
          break;
        case QUANT_ARRAY_TRIE:
          Query<QuantArrayTrieModel>(file, config, sentence_context, printer);
          break;
        default:
          std::cerr << "Unrecognized kenlm model type " << model_type << std::endl;
          abort();
      }
#ifdef WITH_NPLM
    } else if (lm::np::Model::Recognize(file)) {
      lm::np::Model model(file);
      Query<lm::np::Model, lm::ngram::QueryPrinter>(model, sentence_context, printer);
      Query<lm::np::Model, lm::ngram::QueryPrinter>(model, sentence_context, printer);
#endif
    } else {
      Query<ProbingModel>(file, config, sentence_context, printer);
    }
    util::PrintUsage(std::cerr);
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  return 0;
}
