#include "lm/builder/print.hh"

#include "util/file.hh"
#include "util/mmap.hh"

#include <string.h>

namespace lm { namespace builder {

VocabReconstitute::VocabReconstitute(int fd) {
  uint64_t size = util::SizeFile(fd);
  UTIL_THROW_IF(util::kBadSize == size, util::ErrnoException, "Vocabulary file should be sizeable");
  util::MapRead(util::POPULATE_OR_READ, fd, 0, size, memory_);
  const char *const start = static_cast<const char*>(memory_.get());
  for (const char *i = start; i != start + size; i += strlen(i) + 1) {
    map_.push_back(i);
  }
}

PrintARPA::PrintARPA(const VocabReconstitute &vocab, const std::vector<uint64_t> counts, std::ostream &out) 
  : vocab_(vocab), out_(out) {
  out_ << "\\data\\\n";
  for (size_t i = 0; i < counts.size(); ++i) {
    out_ << "ngram " << (i+1) << '=' << counts[i] << '\n';
  }
  out_ << '\n';
}

void PrintARPA::Run(const ChainPositions &positions) {
  for (unsigned order = 1; order <= positions.size(); ++order) {
    out_ << "\\" << order << "-grams:" << '\n';
    for (NGramStream stream(positions[order - 1]); stream; ++stream) {
      // Correcting for numerical precision issues.  Take that IRST.  
      out_ << std::min(0.0f, stream->Value().complete.prob) << '\t' << vocab_.Lookup(*stream->begin());
      for (const WordIndex *i = stream->begin() + 1; i != stream->end(); ++i) {
        out_ << ' ' << vocab_.Lookup(*i);
      }
      float backoff = stream->Value().complete.backoff;
      if (backoff != 0.0)
        out_ << '\t' << backoff;
      out_ << '\n';
    }
    out_ << '\n';
  }
  out_ << "\\end\\\n";
}

}} // namespaces
