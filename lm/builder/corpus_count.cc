#include "lm/builder/corpus_count.hh"

#include "lm/builder/ngram.hh"
#include "lm/lm_exception.hh"
#include "lm/word_index.hh"
#include "util/file.hh"
#include "util/file_piece.hh"
#include "util/murmur_hash.hh"
#include "util/stream/chain.hh"
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

class Writer {
  public:
    Writer(std::size_t order, const util::stream::ChainPosition &position) 
      : block_(position), gram_(block_->Get(), order),
        cache_(position.GetChain().BlockSize() / NGram::TotalSize(order), DedupeHash(order), DedupeEquals(order)),
        buffer_(new WordIndex[order - 1]),
        block_size_(position.GetChain().BlockSize()) {
      if (order == 1) {
        // Add <unk>.  AdjustCounts is responsible if order != 1.    
        *gram_.begin() = kUNK;
        gram_.Count() = 0;
        gram_.NextInMemory();
        if (gram_.Base() == static_cast<uint8_t*>(block_->Get()) + block_size_) {
          block_->SetValidSize(block_size_);
          gram_.ReBase((++block_)->Get());
        }
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
      std::pair<Cache::iterator, bool> res(cache_.insert(gram_.begin()));
      if (!res.second) {
        // Already present.  
        NGram already(*res.first, gram_.Order());
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
      cache_.clear();
      block_->SetValidSize(block_size_);
      gram_.ReBase((++block_)->Get());
      std::copy(buffer_.get(), buffer_.get() + gram_.Order() - 1, gram_.begin());
    }

  private:
    util::stream::Link block_;

    NGram gram_;

    // TODO: use linear probing hash table to control memory usage?  
    typedef boost::unordered_set<WordIndex *, DedupeHash, DedupeEquals> Cache;
    Cache cache_;

    // Small buffer to hold existing ngrams when shifting across a block boundary.  
    boost::scoped_array<WordIndex> buffer_;

    const std::size_t block_size_;
};

} // namespace

void CorpusCount::Run(const util::stream::ChainPosition &position) {
  VocabHandout vocab(vocab_write_);
  const WordIndex end_sentence = vocab.Lookup("</s>");
  Writer writer(order_, position);
  try {
    while(true) {
      StringPiece line(from_.ReadLine());
      writer.StartSentence();
      for (util::TokenIter<util::SingleCharacter, true> w(line, ' '); w; ++w) {
        writer.Append(vocab.Lookup(*w));
      }
      writer.Append(end_sentence);
    }
  } catch (const util::EndOfFileException &e) {}
}

} // namespace builder
} // namespace lm
