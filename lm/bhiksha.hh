/* Simple implementation of
 * @inproceedings{bhikshacompression,
 *  author={Bhiksha Raj and Ed Whittaker},
 *  year={2003},
 *  title={Lossless Compression of Language Model Structure and Word Identifiers},
 *  booktitle={Proceedings of IEEE International Conference on Acoustics, Speech and Signal Processing},
 *  pages={388--391},
 *  }
 *
 *  Currently only used for next pointers.  
 */

#ifndef LM_BHIKSHA__
#define LM_BHIKSHA__

#include <stdint.h>
#include <assert.h>

#include "lm/model_type.hh"
#include "lm/trie.hh"
#include "util/bit_packing.hh"
#include "util/sorted_uniform.hh"

namespace lm {
namespace ngram {
struct Config;

namespace trie {

class DontBhiksha {
  public:
    static const ModelType kModelTypeAdd = static_cast<ModelType>(0);

    static void UpdateConfigFromBinary(int /*fd*/, Config &/*config*/) {}

    static uint64_t Size(uint64_t /*max_offset*/, uint64_t /*max_next*/, const Config &/*config*/) { return 0; }

    static uint8_t InlineBits(uint64_t /*max_offset*/, uint64_t max_next, const Config &/*config*/) {
      return util::RequiredBits(max_next);
    }

    DontBhiksha(const void *base, uint64_t max_offset, uint64_t max_next, const Config &config);

    void ReadNext(const void *base, uint64_t bit_offset, uint64_t /*index*/, uint8_t total_bits, NodeRange &out) const {
      out.begin = util::ReadInt57(base, bit_offset, next_.bits, next_.mask);
      out.end = util::ReadInt57(base, bit_offset + total_bits, next_.bits, next_.mask);
      //assert(out.end >= out.begin);
    }

    void WriteNext(void *base, uint64_t bit_offset, uint64_t /*index*/, uint64_t value) {
      util::WriteInt57(base, bit_offset, next_.bits, value);
    }

    void FinishedLoading(const Config &/*config*/) {}

    void LoadedBinary() {}

    uint8_t InlineBits() const { return next_.bits; }

  private:
    util::BitsMask next_;
};

class ArrayBhiksha {
  public:
    static const ModelType kModelTypeAdd = kArrayAdd;

    static void UpdateConfigFromBinary(int fd, Config &config);

    static uint64_t Size(uint64_t max_offset, uint64_t max_next, const Config &config);

    static uint8_t InlineBits(uint64_t max_offset, uint64_t max_next, const Config &config);

    ArrayBhiksha(void *base, uint64_t max_offset, uint64_t max_value, const Config &config);

    void ReadNext(const void *base, uint64_t bit_offset, uint64_t index, uint8_t total_bits, NodeRange &out) const {
      const uint64_t *begin_it = util::BinaryBelow(util::IdentityAccessor<uint64_t>(), offset_begin_, offset_end_, index);
      const uint64_t *end_it;
      for (end_it = begin_it; (end_it < offset_end_) && (*end_it <= index + 1); ++end_it) {}
      --end_it;
      out.begin = ((begin_it - offset_begin_) << next_inline_.bits) | 
        util::ReadInt57(base, bit_offset, next_inline_.bits, next_inline_.mask);
      out.end = ((end_it - offset_begin_) << next_inline_.bits) | 
        util::ReadInt57(base, bit_offset + total_bits, next_inline_.bits, next_inline_.mask);
      //assert(out.end >= out.begin);
    }

    void WriteNext(void *base, uint64_t bit_offset, uint64_t index, uint64_t value) {
      uint64_t encode = value >> next_inline_.bits;
      for (; write_to_ <= offset_begin_ + encode; ++write_to_) *write_to_ = index;
      util::WriteInt57(base, bit_offset, next_inline_.bits, value & next_inline_.mask);
    }

    void FinishedLoading(const Config &config);

    void LoadedBinary();

    uint8_t InlineBits() const { return next_inline_.bits; }

  private:
    const util::BitsMask next_inline_;

    const uint64_t *const offset_begin_;
    const uint64_t *const offset_end_;

    uint64_t *write_to_;

    void *original_base_;
};

} // namespace trie
} // namespace ngram
} // namespace lm

#endif // LM_BHIKSHA__
