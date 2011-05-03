#include "util/file_piece.hh"
#include "util/tokenize_piece.hh"
#include "lm/model.hh"

#include <iostream>

template <class Model> void Convert(const char *name) {
  lm::ngram::Config config;
  config.load_method = util::LAZY;
  Model m(name, config);
  util::FilePiece in(0, "stdin", &std::cerr);
  StringPiece line;
  lm::WordIndex w;
  try {
    while (true) {
      line = in.ReadLine();
      for (util::PieceIterator<' '> i(line); i; ++i) {
        lm::WordIndex w = m.GetVocabulary().Index(*i);
        if (1 != fwrite(&w, sizeof(w), 1, stdout)) UTIL_THROW(util::ErrnoException, "fwrite failed.");
      }
      w = m.GetVocabulary().EndSentence();
      if (1 != fwrite(&w, sizeof(w), 1, stdout)) UTIL_THROW(util::ErrnoException, "fwrite failed.");
    }
  } catch (const util::EndOfFileException &e) {}
}

int main(int argc, char *argv[]) {
  if (argc != 2) UTIL_THROW(util::Exception, "Provide lm file name on command line.");
  lm::ngram::ModelType model_type;
  if (lm::ngram::RecognizeBinary(argv[1], model_type)) {
    switch(model_type) {
      case lm::ngram::HASH_PROBING:
        Convert<lm::ngram::ProbingModel>(argv[1]);
        break;
      case lm::ngram::TRIE_SORTED:
        Convert<lm::ngram::TrieModel>(argv[1]);
        break;
      case lm::ngram::HASH_SORTED:
      default:
        std::cerr << "Unrecognized kenlm model type " << model_type << std::endl;
        abort();
    }
    return 0;
  } else {
    std::cerr << "Convert to binary first." << std::endl;
    return 1;
  }
}
