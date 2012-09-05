#include "builder/ngram.hh"
#include "lm/word_index.hh"
#include "util/file_piece.hh"
#include "util/murmur_hash.hh"
#include "util/tokenize_piece.hh"

#include <boost/unordered_map.hpp>
#include <tpie/file_stream.h>

#include <string>

#include <assert.h>
#include <stdint.h>

namespace lm {
namespace builder {

const WordIndex kBOS = 1;

class VocabHandout {
  public:
    explicit VocabHandout(const char *name);

    WordIndex Lookup(const StringPiece &word);

  private:
    typedef boost::unordered_map<uint64_t, lm::WordIndex> Seen;

    Seen seen_;

    util::scoped_FILE word_list_;
};

template <unsigned N> inline uint64_t hash_value(const NGram<N> &gram, uint64_t seed = 0) {
  return util::MurmurHashNative(gram.w, N * sizeof(WordIndex));
}

// TODO: use different hash table here with controlled memory and sortable.  
template <unsigned N> class HashCombiner {
  public:
    explicit HashCombiner(tpie::temp_file &out) {
      out_.open(out, tpie::access_write);
    }

    void Add(const NGram<N> &gram) const {
      ++cache_[gram];
      if (cache_.size() == limit_) {
        Flush();
      }
    }

    void Flush() {
      CountedNGram<N> copied;
      for (typename Cache::const_iterator i = cache_.begin(); i != cache_.end(); ++i) {
        memcpy(copied.w, i->first.w, N * sizeof(lm::WordIndex));
        copied.count = i->second;
        out_.write(copied);
      }
      cache_.clear();
    }

  private:
    typedef boost::unordered_map<NGram<N>, uint64_t> Cache;
    Cache cache_;

    const std::size_t limit_;

    tpie::file_stream<NGram<N> > out_;
};

template <unsigned N> void ReadInput(const char *from_name, const char *vocab_write, tpie::temp_file &out) {
  util::FilePiece from(from_name);
  VocabHandout vocab(vocab_write);
  HashCombiner<N> combiner(out);
  NGram<N> gram;
  for (unsigned int i = 0; i < N; ++i) gram.w[i] = kBOS;
  try {
    while(true) {
      StringPiece line(from.ReadLine());
      for (util::TokenIter<util::SingleCharacter, true> w(line, ' '); w; ++w) {
        for (unsigned int i = 1; i < N; ++i) gram.w[i-1] = gram.w[i];
        gram.w[N-1] = vocab.Lookup(*w);
        combiner.Add(gram);
      }
    }
  } catch (const util::EndOfFileException &e) {}
  combiner.Flush();
}

} // namespace builder
} // namespace lm
