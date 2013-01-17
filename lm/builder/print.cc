#include "lm/builder/print.hh"

#include "util/double-conversion/double-conversion.h"
#include "util/double-conversion/utils.h"
#include "util/file.hh"
#include "util/mmap.hh"
#include "util/scoped.hh"
#include "util/stream/timer.hh"

#define BOOST_LEXICAL_CAST_ASSUME_C_LOCALE
#include <boost/lexical_cast.hpp>

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

namespace {
class OutputManager {
  public:
    static const std::size_t kOutBuf = 1048576;

    // Does not take ownership of out.
    explicit OutputManager(int out)
      : buf_(util::MallocOrThrow(kOutBuf)),
        builder_(static_cast<char*>(buf_.get()), kOutBuf),
        // Mostly the default but with inf instead.  And no flags.
        convert_(double_conversion::DoubleToStringConverter::NO_FLAGS, "inf", "NaN", 'e', -6, 21, 6, 0),
        fd_(out) {}

    ~OutputManager() {
      Flush();
    }

    OutputManager &operator<<(float value) {
      // Odd, but this is the largest number found in the comments.
      EnsureRemaining(double_conversion::DoubleToStringConverter::kMaxPrecisionDigits + 8);
      convert_.ToShortestSingle(value, &builder_);
      return *this;
    }

    OutputManager &operator<<(StringPiece str) {
      if (str.size() > kOutBuf) {
        Flush();
        util::WriteOrThrow(fd_, str.data(), str.size());
      } else {
        EnsureRemaining(str.size());
        builder_.AddSubstring(str.data(), str.size());
      }
      return *this;
    }

    // Inefficient!
    OutputManager &operator<<(unsigned val) {
      return *this << boost::lexical_cast<std::string>(val);
    }

    OutputManager &operator<<(char c) {
      EnsureRemaining(1);
      builder_.AddCharacter(c);
      return *this;
    }

    void Flush() {
      util::WriteOrThrow(fd_, buf_.get(), builder_.position());
      builder_.Reset();
    }

  private:
    void EnsureRemaining(std::size_t amount) {
      if (static_cast<std::size_t>(builder_.size() - builder_.position()) < amount) {
        Flush();
      }
    }

    util::scoped_malloc buf_;
    double_conversion::StringBuilder builder_;
    double_conversion::DoubleToStringConverter convert_;
    int fd_;
};
} // namespace

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
  UTIL_TIMER("(%w s) Wrote ARPA file\n");
  OutputManager out(out_fd_);
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
