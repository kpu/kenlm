#ifndef LM_VOCAB__
#define LM_VOCAB__

#include "lm/enumerate_vocab.hh"
#include "lm/lm_exception.hh"
#include "lm/virtual_interface.hh"
#include "util/pool.hh"
#include "util/probing_hash_table.hh"
#include "util/sorted_uniform.hh"
#include "util/string_piece.hh"

#include <limits>
#include <string>
#include <vector>

namespace lm {
struct ProbBackoff;
class EnumerateVocab;

namespace ngram {
struct Config;

namespace detail {
uint64_t HashForVocab(const char *str, std::size_t len);
inline uint64_t HashForVocab(const StringPiece &str) {
  return HashForVocab(str.data(), str.length());
}
struct ProbingVocabularyHeader;
} // namespace detail

class WriteWordsWrapper : public EnumerateVocab {
  public:
    WriteWordsWrapper(EnumerateVocab *inner);

    ~WriteWordsWrapper();
    
    void Add(WordIndex index, const StringPiece &str);

    void Write(int fd, uint64_t start);

  private:
    EnumerateVocab *inner_;

    std::string buffer_;
};

// Vocabulary based on sorted uniform find storing only uint64_t values and using their offsets as indices.  
class SortedVocabulary : public base::Vocabulary {
  public:
    SortedVocabulary();

    WordIndex Index(const StringPiece &str) const {
      const uint64_t *found;
      if (util::BoundedSortedUniformFind<const uint64_t*, util::IdentityAccessor<uint64_t>, util::Pivot64>(
            util::IdentityAccessor<uint64_t>(),
            begin_ - 1, 0,
            end_, std::numeric_limits<uint64_t>::max(),
            detail::HashForVocab(str), found)) {
        return found - begin_ + 1; // +1 because <unk> is 0 and does not appear in the lookup table.
      } else {
        return 0;
      }
    }

    // Size for purposes of file writing
    static uint64_t Size(uint64_t entries, const Config &config);

    // Vocab words are [0, Bound())  Only valid after FinishedLoading/LoadedBinary.  
    WordIndex Bound() const { return bound_; }

    // Everything else is for populating.  I'm too lazy to hide and friend these, but you'll only get a const reference anyway.
    void SetupMemory(void *start, std::size_t allocated, std::size_t entries, const Config &config);

    void ConfigureEnumerate(EnumerateVocab *to, std::size_t max_entries);

    WordIndex Insert(const StringPiece &str);

    // Reorders reorder_vocab so that the IDs are sorted.  
    void FinishedLoading(ProbBackoff *reorder_vocab);

    // Trie stores the correct counts including <unk> in the header.  If this was previously sized based on a count exluding <unk>, padding with 8 bytes will make it the correct size based on a count including <unk>.
    std::size_t UnkCountChangePadding() const { return SawUnk() ? 0 : sizeof(uint64_t); }

    bool SawUnk() const { return saw_unk_; }

    void LoadedBinary(bool have_words, int fd, EnumerateVocab *to);

  private:
    uint64_t *begin_, *end_;

    WordIndex bound_;

    WordIndex highest_value_;

    bool saw_unk_;

    EnumerateVocab *enumerate_;

    // Actual strings.  Used only when loading from ARPA and enumerate_ != NULL 
    util::Pool string_backing_;

    std::vector<StringPiece> strings_to_enumerate_;
};

#pragma pack(push)
#pragma pack(4)
struct ProbingVocabuaryEntry {
  uint64_t key;
  WordIndex value;

  typedef uint64_t Key;
  uint64_t GetKey() const {
    return key;
  }

  static ProbingVocabuaryEntry Make(uint64_t key, WordIndex value) {
    ProbingVocabuaryEntry ret;
    ret.key = key;
    ret.value = value;
    return ret;
  }
};
#pragma pack(pop)

// Vocabulary storing a map from uint64_t to WordIndex. 
class ProbingVocabulary : public base::Vocabulary {
  public:
    ProbingVocabulary();

    WordIndex Index(const StringPiece &str) const {
      Lookup::ConstIterator i;
      return lookup_.Find(detail::HashForVocab(str), i) ? i->value : 0;
    }

    static uint64_t Size(uint64_t entries, const Config &config);

    // Vocab words are [0, Bound()).  
    WordIndex Bound() const { return bound_; }

    // Everything else is for populating.  I'm too lazy to hide and friend these, but you'll only get a const reference anyway.
    void SetupMemory(void *start, std::size_t allocated, std::size_t entries, const Config &config);

    void ConfigureEnumerate(EnumerateVocab *to, std::size_t max_entries);

    WordIndex Insert(const StringPiece &str);

    template <class Weights> void FinishedLoading(Weights * /*reorder_vocab*/) {
      InternalFinishedLoading();
    }

    std::size_t UnkCountChangePadding() const { return 0; }

    bool SawUnk() const { return saw_unk_; }

    void LoadedBinary(bool have_words, int fd, EnumerateVocab *to);

  private:
    void InternalFinishedLoading();

    typedef util::ProbingHashTable<ProbingVocabuaryEntry, util::IdentityHash> Lookup;

    Lookup lookup_;

    WordIndex bound_;

    bool saw_unk_;

    EnumerateVocab *enumerate_;

    detail::ProbingVocabularyHeader *header_;
};

void MissingUnknown(const Config &config) throw(SpecialWordMissingException);
void MissingSentenceMarker(const Config &config, const char *str) throw(SpecialWordMissingException);

template <class Vocab> void CheckSpecials(const Config &config, const Vocab &vocab) throw(SpecialWordMissingException) {
  if (!vocab.SawUnk()) MissingUnknown(config);
  if (vocab.BeginSentence() == vocab.NotFound()) MissingSentenceMarker(config, "<s>");
  if (vocab.EndSentence() == vocab.NotFound()) MissingSentenceMarker(config, "</s>");
}

} // namespace ngram
} // namespace lm

#endif // LM_VOCAB__
