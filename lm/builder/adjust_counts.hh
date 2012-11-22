#ifndef BUILDER_ADJUST_COUNTS__
#define BUILDER_ADJUST_COUNTS__

#include "lm/builder/ngram.hh"
#include "lm/builder/multi_file_stream.hh"

#include <tpie/file_stream.h>

namespace lm {
namespace builder {

template <unsigned N> class Ratchet {
  public:
    explicit Ratchet(MultiFileStream<N, CountedNGram> &streams) : 
      lower_(streams.GetTail()), out_(streams.GetHead()), seen_(std::numeric_limits<lm::WordIndex>::max()) {
      record_.count = 0;
    }

    bool Check(const WordIndex *i) {
      if (lower_.Check(i + 1)) {
        if (*i == seen_) return true;
        ++record_.count;
      } else {
        out_.write(record_);

        record_.count = 1;
      }
      seen_ = *i;
      return false;
    }

  private:
    Ratchet<N-1> lower_;

    tpie::file_stream<CountedNGram<N> > &out_;

    WordIndex seen_;

    CountedNGram<N> record_;
};

template <> struct Ratchet<0> {
  explicit Ratchet(MultiFileStream<0, CountedNGram> &streams) : seen_(std::numeric_limits<lm::WordIndex>::max()) {}

  // Did the unigram change?  
  bool Check(const WordIndex *i) {
    if (*i == seen_) {
      return true;
    } else {
      seen_ = *i;
      return false;
    }
  }

  WordIndex seen_;
};

template <unsigned N> void AdjustCounts(const char *in) {
/*  tpie::file_stream<NGram<N> > input(in, tpie::access_read);
  MultiFileStream <N, CountedNGram> mfs(in, tpie::access_read_write);
  while (input.can_read()) {

  }*/
}

} // namespace builder
} // namespace lm

#endif // BUILDER_ADJUST_COUNTS__

