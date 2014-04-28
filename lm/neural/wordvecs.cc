#include "lm/neural/wordvecs.hh"

#include "util/file_piece.hh"

namespace lm { namespace neural {

WordVecs::WordVecs(util::FilePiece &f) {
  const unsigned long lines = f.ReadULong();
  const std::size_t vocab_mem = ngram::ProbingVocabulary::Size(lines, 1.5);
  vocab_backing_.reset(util::CallocOrThrow(vocab_mem));
  vocab_.SetupMemory(vocab_backing_.get(), vocab_mem);
  const unsigned long width = f.ReadULong();
  vecs_.resize(width, lines);
  for (unsigned long i = 0; i < lines; ++i) {
    WordIndex column = vocab_.Insert(f.ReadDelimited());
    for (unsigned int row = 0; row < width; ++row) {
      vecs_(row,column) = f.ReadFloat();
    }
  }
  vocab_.FinishedLoading();
}

}} // namespaces
