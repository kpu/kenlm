#ifndef BUILDER_NGRAM__
#define BUILDER_NGRAM__

#include "lm/word_index.hh"

#include <stdint.h>
#include <string.h>

namespace lm {
namespace builder {

struct Uninterpolated {
  float prob;  // Uninterpolated probability.
  float gamma; // Interpolation weight for lower order.
};

struct Interpolated {
  float prob;  // p(w_n | w_1^{n-1})
  float lower; // p(w_n | w_2^{n-1})
};

union Payload {
  uint64_t count;
  Uninterpolated uninterp;
  Interpolated interp;
};

class NGram {
  public:
    NGram(void *begin, std::size_t order) 
      : begin_(static_cast<WordIndex*>(begin)), end_(begin_ + order) {}

    // Lower-case in deference to STL.  
    const WordIndex *begin() const { return begin_; }
    WordIndex *begin() { return begin_; }
    const WordIndex *end() const { return end_; }
    WordIndex *end() { return end_; }

    const Payload &Value() const { return *reinterpret_cast<const Payload *>(end_); }
    Payload &Value() { return *reinterpret_cast<Payload *>(end_); }

    uint64_t &Count() { return Value().count; }
    const uint64_t Count() const { return Value().count; }

    std::size_t Order() const { return end_ - begin_; }

    static std::size_t Size(std::size_t order) {
      return order * sizeof(WordIndex) + sizeof(Payload);
    }
    std::size_t Size() const {
      // Compiler should optimize this.  
      return Size(Order());
    }

    // Advance by size.  
    NGram &operator++() {
      std::size_t change = Size();
      begin_ = reinterpret_cast<WordIndex*>(reinterpret_cast<uint8_t*>(begin_) + change);
      end_ = reinterpret_cast<WordIndex*>(reinterpret_cast<uint8_t*>(end_) + change);
      return *this;
    }
    
  private:
    WordIndex *begin_, *end_;
};

const WordIndex kBOS = 1;

} // namespace builder
} // namespace lm

#endif // BUILDER_NGRAM__
