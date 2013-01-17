#include "lm/builder/corpus_count.hh"

#include "lm/builder/ngram.hh"
#include "lm/lm_exception.hh"
#include "lm/word_index.hh"
#include "util/file.hh"
#include "util/file_piece.hh"
#include "util/murmur_hash.hh"
#include "util/probing_hash_table.hh"
#include "util/scoped.hh"
#include "util/stream/chain.hh"
#include "util/stream/timer.hh"
#include "util/tokenize_piece.hh"

#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include <functional>

#include <stdint.h>

namespace lm {
namespace builder {
namespace {

class VocabHandout {
  public:
    explicit VocabHandout(int fd) {
      util::scoped_fd duped(util::DupOrThrow(fd));
      word_list_.reset(util::FDOpenOrThrow(duped));
      
      Lookup("<unk>"); // Force 0
      Lookup("<s>"); // Force 1
      Lookup("</s>"); // Force 2
    }

    WordIndex Lookup(const StringPiece &word) {
      uint64_t hashed = util::MurmurHashNative(word.data(), word.size());
      std::pair<Seen::iterator, bool> ret(seen_.insert(std::pair<uint64_t, lm::WordIndex>(hashed, seen_.size())));
      if (ret.second) {
        char null_delimit = 0;
        util::WriteOrThrow(word_list_.get(), word.data(), word.size());
        util::WriteOrThrow(word_list_.get(), &null_delimit, 1);
        UTIL_THROW_IF(seen_.size() >= std::numeric_limits<lm::WordIndex>::max(), VocabLoadException, "Too many vocabulary words.  Change WordIndex to uint64_t in lm/word_index.hh.");
      }
      return ret.first->second;
    }

    WordIndex Size() const {
      return seen_.size();
    }

  private:
    typedef boost::unordered_map<uint64_t, lm::WordIndex> Seen;

    Seen seen_;

    util::scoped_FILE word_list_;
};

class DedupeHash : public std::unary_function<const WordIndex *, bool> {
  public:
    explicit DedupeHash(std::size_t order) : size_(order * sizeof(WordIndex)) {}

    std::size_t operator()(const WordIndex *start) const {
      return util::MurmurHashNative(start, size_);
    }
    
  private:
    const std::size_t size_;
};

class DedupeEquals : public std::binary_function<const WordIndex *, const WordIndex *, bool> {
  public:
    explicit DedupeEquals(std::size_t order) : size_(order * sizeof(WordIndex)) {}
    
    bool operator()(const WordIndex *first, const WordIndex *second) const {
      return !memcmp(first, second, size_);
    } 
    
  private:
    const std::size_t size_;
};

struct DedupeEntry {
  typedef WordIndex *Key;
  Key GetKey() const { return key; }
  Key key;
  static DedupeEntry Construct(WordIndex *at) {
    DedupeEntry ret;
    ret.key = at;
    return ret;
  }
};

typedef util::ProbingHashTable<DedupeEntry, DedupeHash, DedupeEquals> Dedupe;

const float kProbingMultiplier = 1.5;

class Writer {
  public:
    Writer(std::size_t order, const util::stream::ChainPosition &position, void *dedupe_mem, std::size_t dedupe_mem_size) 
      : block_(position), gram_(block_->Get(), order),
        dedupe_invalid_(order, std::numeric_limits<WordIndex>::max()),
        dedupe_(dedupe_mem, dedupe_mem_size, &dedupe_invalid_[0], DedupeHash(order), DedupeEquals(order)),
        buffer_(new WordIndex[order - 1]),
        block_size_(position.GetChain().BlockSize()) {
      dedupe_.Clear(DedupeEntry::Construct(&dedupe_invalid_[0]));
      assert(Dedupe::Size(position.GetChain().BlockSize() / position.GetChain().EntrySize(), kProbingMultiplier) == dedupe_mem_size);
      if (order == 1) {
        // Add special words.  AdjustCounts is responsible if order != 1.    
        AddUnigramWord(kUNK);
        AddUnigramWord(kBOS);
      }
    }

    ~Writer() {
      block_->SetValidSize(reinterpret_cast<const uint8_t*>(gram_.begin()) - static_cast<const uint8_t*>(block_->Get()));
      (++block_).Poison();
    }

    // Write context with a bunch of <s>
    void StartSentence() {
      for (WordIndex *i = gram_.begin(); i != gram_.end() - 1; ++i) {
        *i = kBOS;
      }
    }

    void Append(WordIndex word) {
      *(gram_.end() - 1) = word;
      Dedupe::MutableIterator at;
      bool found = dedupe_.FindOrInsert(DedupeEntry::Construct(gram_.begin()), at);
      if (found) {
        // Already present.
        NGram already(at->key, gram_.Order());
        ++(already.Count());
        // Shift left by one.
        memmove(gram_.begin(), gram_.begin() + 1, sizeof(WordIndex) * (gram_.Order() - 1));
        return;
      }
      // Complete the write.  
      gram_.Count() = 1;
      // Prepare the next n-gram.  
      if (reinterpret_cast<uint8_t*>(gram_.begin()) + gram_.TotalSize() != static_cast<uint8_t*>(block_->Get()) + block_size_) {
        NGram last(gram_);
        gram_.NextInMemory();
        std::copy(last.begin() + 1, last.end(), gram_.begin());
        return;
      }
      // Block end.  Need to store the context in a temporary buffer.  
      std::copy(gram_.begin() + 1, gram_.end(), buffer_.get());
      dedupe_.Clear(DedupeEntry::Construct(&dedupe_invalid_[0]));
      block_->SetValidSize(block_size_);
      gram_.ReBase((++block_)->Get());
      std::copy(buffer_.get(), buffer_.get() + gram_.Order() - 1, gram_.begin());
    }

  private:
    void AddUnigramWord(WordIndex index) {
      *gram_.begin() = index;
      gram_.Count() = 0;
      gram_.NextInMemory();
      if (gram_.Base() == static_cast<uint8_t*>(block_->Get()) + block_size_) {
        block_->SetValidSize(block_size_);
        gram_.ReBase((++block_)->Get());
      }
    }

    util::stream::Link block_;

    NGram gram_;

    // This is the memory behind the invalid value in dedupe_.
    std::vector<WordIndex> dedupe_invalid_;
    // Hash table combiner implementation.
    Dedupe dedupe_;

    // Small buffer to hold existing ngrams when shifting across a block boundary.  
    boost::scoped_array<WordIndex> buffer_;

    const std::size_t block_size_;
};

} // namespace

float CorpusCount::DedupeMultiplier(std::size_t order) {
  return kProbingMultiplier * static_cast<float>(sizeof(DedupeEntry)) / static_cast<float>(NGram::TotalSize(order));
}

CorpusCount::CorpusCount(util::FilePiece &from, int vocab_write, uint64_t &token_count, WordIndex &type_count, std::size_t entries_per_block) 
  : from_(from), vocab_write_(vocab_write), token_count_(token_count), type_count_(type_count),
    dedupe_mem_size_(Dedupe::Size(entries_per_block, kProbingMultiplier)),
    dedupe_mem_(util::MallocOrThrow(dedupe_mem_size_)) {
  token_count_ = 0;
  type_count_ = 0;
}

void CorpusCount::Run(const util::stream::ChainPosition &position) {
  UTIL_TIMER("(%w s) Counted n-grams\n");

  VocabHandout vocab(vocab_write_);
  const WordIndex end_sentence = vocab.Lookup("</s>");
  Writer writer(NGram::OrderFromSize(position.GetChain().EntrySize()), position, dedupe_mem_.get(), dedupe_mem_size_);
  uint64_t count = 0;
  try {
    while(true) {
      StringPiece line(from_.ReadLine());
      writer.StartSentence();
      for (util::TokenIter<util::AnyCharacter, true> w(line, " \t"); w; ++w) {
        WordIndex word = vocab.Lookup(*w);
        UTIL_THROW_IF(word <= 2, FormatLoadException, "Special word " << *w << " is not allowed in the corpus.  I plan to support models containing <unk> in the future.");
        writer.Append(word);
        ++count;
      }
      writer.Append(end_sentence);
    }
  } catch (const util::EndOfFileException &e) {}
  token_count_ = count;
  type_count_ = vocab.Size();
}

} // namespace builder
} // namespace lm
