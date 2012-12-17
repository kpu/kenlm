#ifndef LM_BUILDER_PRINT__
#define LM_BUILDER_PRINT__

#include "lm/builder/ngram.hh"
#include "lm/builder/multi_stream.hh"
#include "util/file.hh"
#include "util/mmap.hh"

#include <ostream>

#include <assert.h>

namespace lm { namespace builder {

class VocabReconstitute {
  public:
    // fd must be alive for life of this object; does not take ownership.
    explicit VocabReconstitute(int fd);

    const char *Lookup(WordIndex index) const {
      assert(index < map_.size());
      return map_[index];
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
  to << payload.uninterp.prob << ' ' << payload.uninterp.gamma;
}
template <> inline void PrintPayload<Interpolated>(std::ostream &to, const Payload &payload) {
  to << payload.interp.prob << ' ' << payload.interp.lower;
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
          to_ << ' ' << vocab_.Lookup(*w);
        }
        to_ << '\n';
      }
    }

    const VocabReconstitute &vocab_;
    std::ostream &to_;
};

}} // namespaces
#endif // LM_BUILDER_PRINT__
