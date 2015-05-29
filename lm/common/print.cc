#include "lm/common/print.hh"

#include "lm/common/ngram_stream.hh"
#include "util/fake_ofstream.hh"
#include "util/file.hh"
#include "util/mmap.hh"
#include "util/scoped.hh"

#include <sstream>
#include <cstring>

namespace lm {

VocabReconstitute::VocabReconstitute(int fd) {
  uint64_t size = util::SizeOrThrow(fd);
  util::MapRead(util::POPULATE_OR_READ, fd, 0, size, memory_);
  const char *const start = static_cast<const char*>(memory_.get());
  const char *i;
  for (i = start; i != start + size; i += strlen(i) + 1) {
    map_.push_back(i);
  }
  // Last one for LookupPiece.
  map_.push_back(i);
}

void PrintARPA::Run(const util::stream::ChainPositions &positions) {
  VocabReconstitute vocab(vocab_fd_);
  util::FakeOFStream out(out_fd_);
  out << "\\data\\\n";
  for (size_t i = 0; i < positions.size(); ++i) {
    out << "ngram " << (i+1) << '=' << counts_[i] << '\n';
  }
  out << '\n';

  for (unsigned order = 1; order <= positions.size(); ++order) {
    out << "\\" << order << "-grams:" << '\n';
    for (NGramStream<ProbBackoff> stream(positions[order - 1]); stream; ++stream) {
      // Correcting for numerical precision issues.  Take that IRST.
      out << stream->Value().prob << '\t' << vocab.Lookup(*stream->begin());
      for (const WordIndex *i = stream->begin() + 1; i != stream->end(); ++i) {
        out << ' ' << vocab.Lookup(*i);
      }
      if (order != positions.size())
        out << '\t' << stream->Value().backoff;
      out << '\n';

    }
    out << '\n';
  }
  out << "\\end\\\n";
}

} // namespace lm
