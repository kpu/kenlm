#include "lm/model.hh"
#include "util/file_stream.hh"
#include "util/file.hh"
#include "util/file_piece.hh"
#include "util/usage.hh"

#include <stdint.h>

namespace {

template <class Model, class Width> void ConvertToBytes(const Model &model, int fd_in) {
  util::FilePiece in(fd_in);
  util::FileStream out(1);
  Width width;
  StringPiece word;
  const Width end_sentence = (Width)model.GetVocabulary().EndSentence();
  while (true) {
    while (in.ReadWordSameLine(word)) {
      width = (Width)model.GetVocabulary().Index(word);
      out.write(&width, sizeof(Width));
    }
    if (!in.ReadLineOrEOF(word)) break;
    out.write(&end_sentence, sizeof(Width));
  }
}

template <class Model, class Width> void QueryFromBytes(const Model &model, int fd_in) {
  lm::ngram::State state[3];
  const lm::ngram::State *const begin_state = &model.BeginSentenceState();
  const lm::ngram::State *next_state = begin_state;
  Width kEOS = model.GetVocabulary().EndSentence();
  Width buf[4096];

  uint64_t completed = 0;
  double loaded = util::CPUTime();

  std::cout << "CPU_to_load: " << loaded << std::endl;

  // Numerical precision: batch sums.
  double total = 0.0;
  while (std::size_t got = util::ReadOrEOF(fd_in, buf, sizeof(buf))) {
    float sum = 0.0;
    UTIL_THROW_IF2(got % sizeof(Width), "File size not a multiple of vocab id size " << sizeof(Width));
    got /= sizeof(Width);
    completed += got;
    // Do even stuff first.
    const Width *even_end = buf + (got & ~1);
    // Alternating states
    const Width *i;
    for (i = buf; i != even_end;) {
      sum += model.FullScore(*next_state, *i, state[1]).prob;
      next_state = (*i++ == kEOS) ? begin_state : &state[1];
      sum += model.FullScore(*next_state, *i, state[0]).prob;
      next_state = (*i++ == kEOS) ? begin_state : &state[0];
    }
    // Odd corner case.
    if (got & 1) {
      sum += model.FullScore(*next_state, *i, state[2]).prob;
      next_state = (*i++ == kEOS) ? begin_state : &state[2];
    }
    total += sum;
  }
  double after = util::CPUTime();
  std::cerr << "Probability sum is " << total << std::endl;
  std::cout << "Queries: " << completed << std::endl;
  std::cout << "CPU_excluding_load: " << (after - loaded) << "\nCPU_per_query: " << ((after - loaded) / static_cast<double>(completed)) << std::endl;
  std::cout << "RSSMax: " << util::RSSMax() << std::endl;
}

template <class Model, class Width> void DispatchFunction(const Model &model, bool query) {
  if (query) {
    QueryFromBytes<Model, Width>(model, 0);
  } else {
    ConvertToBytes<Model, Width>(model, 0);
  }
}

template <class Model> void DispatchWidth(const char *file, bool query) {
  lm::ngram::Config config;
  config.load_method = util::READ;
  std::cerr << "Using load_method = READ." << std::endl;
  Model model(file, config);
  lm::WordIndex bound = model.GetVocabulary().Bound();
  if (bound <= 256) {
    DispatchFunction<Model, uint8_t>(model, query);
  } else if (bound <= 65536) {
    DispatchFunction<Model, uint16_t>(model, query);
  } else if (bound <= (1ULL << 32)) {
    DispatchFunction<Model, uint32_t>(model, query);
  } else {
    DispatchFunction<Model, uint64_t>(model, query);
  }
}

void Dispatch(const char *file, bool query) {
  using namespace lm::ngram;
  lm::ngram::ModelType model_type;
  if (lm::ngram::RecognizeBinary(file, model_type)) {
    switch(model_type) {
      case PROBING:
        DispatchWidth<lm::ngram::ProbingModel>(file, query);
        break;
      case REST_PROBING:
        DispatchWidth<lm::ngram::RestProbingModel>(file, query);
        break;
      case TRIE:
        DispatchWidth<lm::ngram::TrieModel>(file, query);
        break;
      case QUANT_TRIE:
        DispatchWidth<lm::ngram::QuantTrieModel>(file, query);
        break;
      case ARRAY_TRIE:
        DispatchWidth<lm::ngram::ArrayTrieModel>(file, query);
        break;
      case QUANT_ARRAY_TRIE:
        DispatchWidth<lm::ngram::QuantArrayTrieModel>(file, query);
        break;
      default:
        UTIL_THROW(util::Exception, "Unrecognized kenlm model type " << model_type);
    }
  } else {
    UTIL_THROW(util::Exception, "Binarize before running benchmarks.");
  }
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 3 || (strcmp(argv[1], "vocab") && strcmp(argv[1], "query"))) {
    std::cerr
      << "Benchmark program for KenLM.  Intended usage:\n"
      << "#Convert text to vocabulary ids offline.  These ids are tied to a model.\n"
      << argv[0] << " vocab $model <$text >$text.vocab\n"
      << "#Ensure files are in RAM.\n"
      << "cat $text.vocab $model >/dev/null\n"
      << "#Timed query against the model.\n"
      << argv[0] << " query $model <$text.vocab\n";
    return 1;
  }
  Dispatch(argv[2], !strcmp(argv[1], "query"));
  return 0;
}
