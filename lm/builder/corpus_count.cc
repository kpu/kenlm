#include "lm/builder/corpus_count.hh"

#include "lm/builder/ngram.hh"
#include "lm/lm_exception.hh"
#include "lm/word_index.hh"
#include "util/fake_ofstream.hh"
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

#pragma pack(push)
#pragma pack(4)
struct VocabEntry {
  typedef uint64_t Key;

  uint64_t GetKey() const { return key; }
  void SetKey(uint64_t to) { key = to; }

  uint64_t key;
  lm::WordIndex value;
};
#pragma pack(pop)

const float kProbingMultiplier = 1.5;

class VocabHandout {
  public:
    static std::size_t MemUsage(WordIndex initial_guess) {
      if (initial_guess < 2) initial_guess = 2;
      return util::CheckOverflow(Table::Size(initial_guess, kProbingMultiplier));
    }

    explicit VocabHandout(int fd, WordIndex initial_guess) :
        table_backing_(util::CallocOrThrow(MemUsage(initial_guess))),
        table_(table_backing_.get(), MemUsage(initial_guess)),
        double_cutoff_(std::max<std::size_t>(initial_guess * 1.1, 1)),
        word_list_(fd) {
      Lookup("<unk>"); // Force 0
      Lookup("<s>"); // Force 1
      Lookup("</s>"); // Force 2
    }

    WordIndex Lookup(const StringPiece &word) {
      VocabEntry entry;
      entry.key = util::MurmurHashNative(word.data(), word.size());
      entry.value = table_.SizeNoSerialization();

      Table::MutableIterator it;
      if (table_.FindOrInsert(entry, it))
        return it->value;
      word_list_ << word << '\0';
      UTIL_THROW_IF(Size() >= std::numeric_limits<lm::WordIndex>::max(), VocabLoadException, "Too many vocabulary words.  Change WordIndex to uint64_t in lm/word_index.hh.");
      if (Size() >= double_cutoff_) {
        table_backing_.call_realloc(table_.DoubleTo());
        table_.Double(table_backing_.get());
        double_cutoff_ *= 2;
      }
      return entry.value;
    }

    WordIndex Size() const {
      return table_.SizeNoSerialization();
    }

  private:
    // TODO: factor out a resizable probing hash table.
    // TODO: use mremap on linux to get all zeros on resizes.
    util::scoped_malloc table_backing_;

    typedef util::ProbingHashTable<VocabEntry, util::IdentityHash> Table;
    Table table_;

    std::size_t double_cutoff_;
    
    util::FakeOFStream word_list_;
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
  void SetKey(WordIndex *to) { key = to; }
  Key key;
  static DedupeEntry Construct(WordIndex *at) {
    DedupeEntry ret;
    ret.key = at;
    return ret;
  }
};

typedef util::ProbingHashTable<DedupeEntry, DedupeHash, DedupeEquals> Dedupe;

class Writer {
  public:
    Writer(std::size_t order, const util::stream::ChainPosition &position, void *dedupe_mem, std::size_t dedupe_mem_size) 
      : block_(position), gram_(block_->Get(), order),
        dedupe_invalid_(order, std::numeric_limits<WordIndex>::max()),
        dedupe_(dedupe_mem, dedupe_mem_size, &dedupe_invalid_[0], DedupeHash(order), DedupeEquals(order)),
        buffer_(new WordIndex[order - 1]),
        block_size_(position.GetChain().BlockSize()) {
      dedupe_.Clear();
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
      dedupe_.Clear();
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

std::size_t CorpusCount::VocabUsage(std::size_t vocab_estimate) {
  return VocabHandout::MemUsage(vocab_estimate);
}

CorpusCount::CorpusCount(util::FilePiece &from, int vocab_write, uint64_t &token_count, WordIndex &type_count, std::size_t entries_per_block) 
  : from_(from), vocab_write_(vocab_write), token_count_(token_count), type_count_(type_count),
    dedupe_mem_size_(Dedupe::Size(entries_per_block, kProbingMultiplier)),
    dedupe_mem_(util::MallocOrThrow(dedupe_mem_size_)) {
}

void CorpusCount::Run(const util::stream::ChainPosition &position) {
  UTIL_TIMER("(%w s) Counted n-grams\n");

  VocabHandout vocab(vocab_write_, type_count_);
  token_count_ = 0;
  type_count_ = 0;
  const WordIndex end_sentence = vocab.Lookup("</s>");
  Writer writer(NGram::OrderFromSize(position.GetChain().EntrySize()), position, dedupe_mem_.get(), dedupe_mem_size_);
  uint64_t count = 0;
  StringPiece delimiters("\0\t\r ", 4);
  try {
    while(true) {
      StringPiece line(from_.ReadLine());
      writer.StartSentence();
      for (util::TokenIter<util::AnyCharacter, true> w(line, delimiters); w; ++w) {
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
