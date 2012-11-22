#include "lm/builder/ngram.hh"
#include "lm/word_index.hh"
#include "util/file_piece.hh"
#include "util/murmur_hash.hh"
#include "util/tokenize_piece.hh"

#include <boost/unordered_map.hpp>
#include <tpie/file_stream.h>

#include <iostream>
#include <string>

#include <assert.h>
#include <stdint.h>

namespace lm {
namespace builder {

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
    explicit HashCombiner(const char *name, std::size_t limit) : limit_(limit) {
      out_.open(name, tpie::access_write);
    }

    void Add(const NGram<N> &gram) {
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

    tpie::file_stream<CountedNGram<N> > out_;
};

template <unsigned N> inline void AppendWord(NGram<N> &gram, WordIndex word, HashCombiner<N> &to) {
  for (unsigned int i = 1; i < N; ++i) gram.w[i-1] = gram.w[i];
  gram.w[N-1] = word;
  to.Add(gram);
}

template <unsigned N> void CorpusCount(const char *base_name) {
  util::FilePiece from(0, "stdin", &std::cerr);
  std::string vocab_name(base_name);
  vocab_name += "_vocab";
  VocabHandout vocab(vocab_name.c_str());
  const WordIndex end_sentence = vocab.Lookup("</s>");
  std::string combiner_name(base_name);
  combiner_name += "_counts";
  HashCombiner<N> combiner(combiner_name.c_str(), 100);
  NGram<N> gram;
  try {
    while(true) {
      StringPiece line(from.ReadLine());
      for (unsigned int i = 0; i < N; ++i) gram.w[i] = kBOS;
      for (util::TokenIter<util::SingleCharacter, true> w(line, ' '); w; ++w) {
        AppendWord(gram, vocab.Lookup(*w), combiner);
      }
      AppendWord(gram, end_sentence, combiner);
    }
  } catch (const util::EndOfFileException &e) {}
  combiner.Flush();
}

} // namespace builder
} // namespace lm
