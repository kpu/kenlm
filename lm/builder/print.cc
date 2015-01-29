#include "lm/builder/print.hh"

#include "util/fake_ofstream.hh"
#include "util/file.hh"
#include "util/mmap.hh"
#include "util/scoped.hh"
#include "util/stream/timer.hh"

#include <sstream>

#include <string.h>

namespace lm { namespace builder {

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
  VocabReconstitute vocab(GetVocabFD());

  // Write header.  TODO: integers in FakeOFStream.
  {
    std::stringstream stream;
    if (verbose_header_) {
      stream << "# Input file: " << GetHeader().input_file << '\n';
      stream << "# Token count: " << GetHeader().token_count << '\n';
      stream << "# Smoothing: Modified Kneser-Ney" << '\n';
    }
    stream << "\\data\\\n";
    for (size_t i = 0; i < positions.size(); ++i) {
      stream << "ngram " << (i+1) << '=' << GetHeader().counts_pruned[i] << '\n';
    }
    stream << '\n';
    std::string as_string(stream.str());
    util::WriteOrThrow(out_fd_.get(), as_string.data(), as_string.size());
  }

  util::FakeOFStream out(out_fd_.get());
  for (unsigned order = 1; order <= positions.size(); ++order) {
    out << "\\" << order << "-grams:" << '\n';
    for (NGramStream stream(positions[order - 1]); stream; ++stream) {
      // Correcting for numerical precision issues.  Take that IRST.
      out << stream->Value().complete.prob << '\t' << vocab.Lookup(*stream->begin());
      for (const WordIndex *i = stream->begin() + 1; i != stream->end(); ++i) {
        out << ' ' << vocab.Lookup(*i);
      }
      if (order != positions.size())
        out << '\t' << stream->Value().complete.backoff;
      out << '\n';
    
    }
    out << '\n';
  }
  out << "\\end\\\n";
}

}} // namespaces
