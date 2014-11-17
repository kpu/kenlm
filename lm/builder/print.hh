#ifndef LM_BUILDER_PRINT_H
#define LM_BUILDER_PRINT_H

#include "lm/builder/ngram.hh"
#include "lm/builder/ngram_stream.hh"
#include "lm/builder/output.hh"
#include "util/fake_ofstream.hh"
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
template <class T> void PrintPayload(util::FakeOFStream &to, const Payload &payload);
template <> inline void PrintPayload<uint64_t>(util::FakeOFStream &to, const Payload &payload) {
  // TODO slow
  to << boost::lexical_cast<std::string>(payload.count);
}
template <> inline void PrintPayload<Uninterpolated>(util::FakeOFStream &to, const Payload &payload) {
  to << log10(payload.uninterp.prob) << ' ' << log10(payload.uninterp.gamma);
}
template <> inline void PrintPayload<ProbBackoff>(util::FakeOFStream &to, const Payload &payload) {
  to << payload.complete.prob << ' ' << payload.complete.backoff;
}

// template parameter is the type stored.  
template <class V> class Print {
  public:
    static void DumpSeparateFiles(const VocabReconstitute &vocab, const std::string &file_base, util::stream::Chains &chains) {
      for (unsigned int i = 0; i < chains.size(); ++i) {
        std::string file(file_base + boost::lexical_cast<std::string>(i));
        chains[i] >> Print(vocab, util::CreateOrThrow(file.c_str()));
      }
    }

    explicit Print(const VocabReconstitute &vocab, int fd) : vocab_(vocab), to_(fd) {}

    void Run(const util::stream::ChainPositions &chains) {
      util::scoped_fd fd(to_);
      util::FakeOFStream out(to_);
      NGramStreams streams(chains);
      for (NGramStream *s = streams.begin(); s != streams.end(); ++s) {
        DumpStream(*s, out);
      }
    }

    void Run(const util::stream::ChainPosition &position) {
      util::scoped_fd fd(to_);
      util::FakeOFStream out(to_);
      NGramStream stream(position);
      DumpStream(stream, out);
    }

  private:
    void DumpStream(NGramStream &stream, util::FakeOFStream &to) {
      for (; stream; ++stream) {
        PrintPayload<V>(to, stream->Value());
        for (const WordIndex *w = stream->begin(); w != stream->end(); ++w) {
          to << ' ' << vocab_.Lookup(*w) << '=' << *w;
        }
        to << '\n';
      }
    }

    const VocabReconstitute &vocab_;
    int to_;
};

class PrintARPA : public OutputHook {
  public:
    explicit PrintARPA(int fd, bool verbose_header)
      : OutputHook(PROB_SEQUENTIAL_HOOK), out_fd_(fd), verbose_header_(verbose_header) {}

    void Run(const util::stream::ChainPositions &positions);

  private:
    util::scoped_fd out_fd_;
    bool verbose_header_;
};

}} // namespaces
#endif // LM_BUILDER_PRINT_H
