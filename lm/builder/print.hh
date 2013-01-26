#ifndef LM_BUILDER_PRINT__
#define LM_BUILDER_PRINT__

#include "lm/builder/ngram.hh"
#include "lm/builder/multi_stream.hh"
#include "lm/builder/header_info.hh"
#include "util/file.hh"
#include "util/mmap.hh"
#include "util/string_piece.hh"

#include <ostream>

#include <assert.h>

// Warning: print routines read all unigrams before all bigrams before all
// trigrams etc.  So if other parts of the chain move jointly, you'll have to
// buffer.  

namespace lm { namespace builder {

class VocabReconstitute {
  public:
    // fd must be alive for life of this object; does not take ownership.
    explicit VocabReconstitute(int fd);

    const char *Lookup(WordIndex index) const {
      assert(index < map_.size() - 1);
      return map_[index];
    }

    StringPiece LookupPiece(WordIndex index) const {
      return StringPiece(map_[index], map_[index + 1] - 1 - map_[index]);
    }

    std::size_t Size() const {
      // There's an extra entry to support StringPiece lengths.
      return map_.size() - 1;
    }

  private:
    util::scoped_memory memory_;
    std::vector<const char*> map_;
};

// Not defined, only specialized.  
template <class T> void PrintPayload(std::ostream &to, const Payload &payload);
template <> inline void PrintPayload<uint64_t>(std::ostream &to, const Payload &payload) {
  to << payload.count;
}
template <> inline void PrintPayload<Uninterpolated>(std::ostream &to, const Payload &payload) {
  to << log10(payload.uninterp.prob) << ' ' << log10(payload.uninterp.gamma);
}
template <> inline void PrintPayload<ProbBackoff>(std::ostream &to, const Payload &payload) {
  to << payload.complete.prob << ' ' << payload.complete.backoff;
}

// template parameter is the type stored.  
template <class V> class Print {
  public:
    explicit Print(const VocabReconstitute &vocab, std::ostream &to) : vocab_(vocab), to_(to) {}

    void Run(const ChainPositions &chains) {
      NGramStreams streams(chains);
      for (NGramStream *s = streams.begin(); s != streams.end(); ++s) {
        DumpStream(*s);
      }
    }

    void Run(const util::stream::ChainPosition &position) {
      NGramStream stream(position);
      DumpStream(stream);
    }

  private:
    void DumpStream(NGramStream &stream) {
      for (; stream; ++stream) {
        PrintPayload<V>(to_, stream->Value());
        for (const WordIndex *w = stream->begin(); w != stream->end(); ++w) {
          to_ << ' ' << vocab_.Lookup(*w) << '=' << *w;
        }
        to_ << '\n';
      }
    }

    const VocabReconstitute &vocab_;
    std::ostream &to_;
};

class PrintARPA {
  public:
    // header_info may be NULL to disable the header.
    // Takes ownership of out_fd upon Run().
    explicit PrintARPA(const VocabReconstitute &vocab, const std::vector<uint64_t> &counts, const HeaderInfo* header_info, int out_fd);

    void Run(const ChainPositions &positions);

  private:
    const VocabReconstitute &vocab_;
    int out_fd_;
};

}} // namespaces
#endif // LM_BUILDER_PRINT__
