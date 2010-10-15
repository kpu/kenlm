#include "lm/trie_node.hh"

#include "util/exception.hh"
#include "util/bit_packing.hh"
#include "util/proxy_iterator.hh"
#include "util/sorted_uniform.hh"

#include <assert.h>

namespace lm {
namespace trie {
namespace {

// Assumes key is first.  
class JustKeyProxy {
  public:
    JustKeyProxy() : inner_(), base_(), key_mask_(), total_bits_() {}

    operator uint64_t() const { return GetKey(); }

    uint64_t GetKey() const {
      uint64_t bit_off = inner_ * static_cast<uint64_t>(total_bits_);
      return util::ReadInt57(base_ + bit_off / 8, bit_off & 7, key_mask_);
    }

  private:
    friend class util::ProxyIterator<JustKeyProxy>;
    friend bool FindBitPacked(const void *base, uint64_t key_mask, uint8_t total_bits, uint64_t begin_index, uint64_t end_index, WordIndex key, uint64_t &at_index);

    JustKeyProxy(const void *base, uint64_t index, uint64_t key_mask, uint8_t total_bits)
      : inner_(index), base_(static_cast<const uint8_t*>(base)), key_mask_(key_mask), total_bits_(total_bits) {}

    // This is a read-only iterator.  
    JustKeyProxy &operator=(const JustKeyProxy &other);

    typedef uint64_t value_type;

    typedef uint64_t InnerIterator;
    uint64_t &Inner() { return inner_; }
    const uint64_t &Inner() const { return inner_; }

    // The address in bits is base_ * 8 + inner_ * total_bits_.  
    uint64_t inner_;
    const uint8_t *const base_;
    const uint64_t key_mask_;
    const uint8_t total_bits_;
};

bool FindBitPacked(const void *base, uint64_t key_mask, uint8_t total_bits, uint64_t begin_index, uint64_t end_index, WordIndex key, uint64_t &at_index) {
  util::ProxyIterator<JustKeyProxy> begin_it(JustKeyProxy(base, begin_index, key_mask, total_bits));
  util::ProxyIterator<JustKeyProxy> end_it(JustKeyProxy(base, end_index, key_mask, total_bits));
  util::ProxyIterator<JustKeyProxy> out;
  if (!util::SortedUniformFind(begin_it, end_it, key, out)) return false;
  at_index = out.Inner();
  return true;
}
} // namespace

std::size_t BitPackedTrie::Size(std::size_t entries, bool is_max_order, uint64_t max_vocab, uint64_t max_ptr) {
  uint8_t total_bits = util::RequiredBits(max_vocab) + 31 + (is_max_order ? 0 : 32) + util::RequiredBits(max_ptr);
  // Extra entry for next pointer at the end.  
  // +7 then / 8 to round up bits and convert to bytes
  // +sizeof(uint64_t) so that ReadInt57 etc don't go segfault.  
  // Note that this waste is O(order), not O(number of ngrams).
  return ((1 + entries) * total_bits + 7) / 8 + sizeof(uint64_t);
}

void BitPackedTrie::Init(void *base, bool is_max_order, uint64_t max_vocab, uint64_t max_ptr) {
  util::BitPackingSanity();
  index_bits_ = util::RequiredBits(max_vocab);
  index_mask_ = (1ULL << index_bits_) - 1ULL;
  prob_bits_ = 31;
  backoff_bits_ = is_max_order ? 0 : 32;
  ptr_bits_ = util::RequiredBits(max_ptr);
  ptr_mask_ = (1ULL << ptr_bits_) - 1ULL;
  // 2^57 is really big.  More than my target users have RAM for.   
  if (index_bits_ > 57) UTIL_THROW(util::Exception, "Sorry, word indices more than " << (1ULL << 57) << " are not implemented.  Edit util/bit_packing.hh and fix the bit packing functions.");
  if (ptr_bits_ > 57) UTIL_THROW(util::Exception, "Sorry, this does not support more than " << (1ULL << 57) << " n-grams of a particular order.  Edit util/bit_packing.hh and fix the bit packing functions.");

  total_bits_ = index_bits_ + prob_bits_ + backoff_bits_ + ptr_bits_;

  base_ = static_cast<uint8_t*>(base);
  insert_pointer_ = 0;
}

bool BitPackedTrie::FindWithNext(const NodeRange &pointer, WordIndex index, float &prob, float &backoff, NodeRange &next_pointer) const {
  uint64_t at_pointer;
  if (!FindBitPacked(base_, index_mask_, total_bits_, pointer.begin, pointer.end, index, at_pointer)) return false;
  at_pointer *= total_bits_;
  at_pointer += index_bits_;
  prob = util::ReadNonPositiveFloat31(base_ + (at_pointer >> 3), at_pointer & 7);
  at_pointer += prob_bits_;
  prob = util::ReadFloat32(base_ + (at_pointer >> 3), at_pointer & 7);
  at_pointer += backoff_bits_;
  next_pointer.begin = util::ReadInt57(base_ + (at_pointer >> 3), at_pointer & 7, ptr_mask_);
  // Read the next entry's pointer.  
  at_pointer += total_bits_;
  next_pointer.end = util::ReadInt57(base_ + (at_pointer >> 3), at_pointer & 7, ptr_mask_);
  return true;
}

void BitPackedTrie::Insert(WordIndex index, float prob, float backoff, uint64_t pointer) {
  assert(index <= index_mask_);
  assert(pointer <= ptr_mask_);
  util::WriteInt57(base_ + (insert_pointer_ >> 3), insert_pointer_ & 7, index);
  insert_pointer_ += index_bits_;
  util::WriteNonPositiveFloat31(base_ + (insert_pointer_ >> 3), insert_pointer_ & 7, prob);
  insert_pointer_ += prob_bits_;
  util::WriteFloat32(base_ + (insert_pointer_ >> 3), insert_pointer_ & 7, backoff);
  insert_pointer_ += backoff_bits_;
  util::WriteInt57(base_ + (insert_pointer_ >> 3), insert_pointer_ & 7, pointer);
  insert_pointer_ += ptr_bits_;
}

void BitPackedTrie::Finish(uint64_t end_pointer) {
  assert(end_pointer <= ptr_mask_);
  uint64_t last_ptr_write = insert_pointer_ + total_bits_ - ptr_bits_;
  util::WriteInt57(base_ + (last_ptr_write >> 3), last_ptr_write & 7, end_pointer);
}

} // namespace trie
} // namespace lm
