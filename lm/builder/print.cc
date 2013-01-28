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

PrintARPA::PrintARPA(const VocabReconstitute &vocab, const std::vector<uint64_t> &counts, const HeaderInfo* header_info, int out_fd) 
  : vocab_(vocab), out_fd_(out_fd) {
  std::stringstream stream;

  if (header_info) {
    stream << "# Input file: " << header_info->input_file << '\n';
    stream << "# Token count: " << header_info->token_count << '\n';
    stream << "# Smoothing: Modified Kneser-Ney" << '\n';
  }
  stream << "\\data\\\n";
  for (size_t i = 0; i < counts.size(); ++i) {
    stream << "ngram " << (i+1) << '=' << counts[i] << '\n';
  }
  stream << '\n';
  std::string as_string(stream.str());
  util::WriteOrThrow(out_fd, as_string.data(), as_string.size());
}

void PrintARPA::Run(const ChainPositions &positions) {
  util::scoped_fd closer(out_fd_);
  UTIL_TIMER("(%w s) Wrote ARPA file\n");
  util::FakeOFStream out(out_fd_);
  for (unsigned order = 1; order <= positions.size(); ++order) {
    out << "\\" << order << "-grams:" << '\n';
    for (NGramStream stream(positions[order - 1]); stream; ++stream) {
      // Correcting for numerical precision issues.  Take that IRST.  
      out << std::min(0.0f, stream->Value().complete.prob) << '\t' << vocab_.Lookup(*stream->begin());
      for (const WordIndex *i = stream->begin() + 1; i != stream->end(); ++i) {
        out << ' ' << vocab_.Lookup(*i);
      }
      float backoff = stream->Value().complete.backoff;
      if (backoff != 0.0)
        out << '\t' << backoff;
      out << '\n';
    }
    out << '\n';
  }
  out << "\\end\\\n";
}

}} // namespaces
