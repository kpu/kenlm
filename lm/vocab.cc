#include "lm/vocab.hh"

#include "lm/weights.hh"
#include "util/joint_sort.hh"
#include "util/murmur_hash.hh"
#include "util/probing_hash_table.hh"

#include <string>

namespace lm {

namespace detail {
uint64_t HashForVocab(const char *str, std::size_t len) {
  // This proved faster than Boost's hash in speed trials: total load time Murmur 67090000, Boost 72210000
  // Chose to use 64A instead of native so binary format will be portable across 64 and 32 bit.  
  return util::MurmurHash64A(str, len, 0);
}
} // namespace detail

namespace {
// Normally static initialization is a bad idea but MurmurHash is pure arithmetic, so this is ok.  
const uint64_t kUnknownHash = detail::HashForVocab("<unk>", 5);
// Sadly some LMs have <UNK>.  
const uint64_t kUnknownCapHash = detail::HashForVocab("<UNK>", 5);
} // namespace

SortedVocabulary::SortedVocabulary() : begin_(NULL), end_(NULL) {}

std::size_t SortedVocabulary::Size(std::size_t entries, float ignored) {
  // Lead with the number of entries.  
  return sizeof(uint64_t) + sizeof(Entry) * entries;
}

void SortedVocabulary::Init(void *start, std::size_t allocated, std::size_t entries) {
  assert(allocated >= Size(entries));
  // Leave space for number of entries.  
  begin_ = reinterpret_cast<Entry*>(reinterpret_cast<uint64_t*>(start) + 1);
  end_ = begin_;
  saw_unk_ = false;
}

WordIndex SortedVocabulary::Insert(const StringPiece &str) {
  uint64_t hashed = detail::HashForVocab(str);
  if (hashed == kUnknownHash || hashed == kUnknownCapHash) {
    saw_unk_ = true;
    return 0;
  }
  end_->key = hashed;
  ++end_;
  // This is 1 + the offset where it was inserted to make room for unk.  
  return end_ - begin_;
}

void SortedVocabulary::FinishedLoading(ProbBackoff *reorder_vocab) {
  util::JointSort(begin_, end_, reorder_vocab + 1);
  SetSpecial(Index("<s>"), Index("</s>"), 0, end_ - begin_ + 1);
  // Save size.  
  *(reinterpret_cast<uint64_t*>(begin_) - 1) = end_ - begin_;
}

void SortedVocabulary::LoadedBinary() {
  end_ = begin_ + *(reinterpret_cast<const uint64_t*>(begin_) - 1);
  SetSpecial(Index("<s>"), Index("</s>"), 0, end_ - begin_ + 1);
}

ProbingVocabulary::ProbingVocabulary() {}

void ProbingVocabulary::Init(void *start, std::size_t allocated, std::size_t entries) {
  lookup_ = Lookup(start, allocated);
  available_ = 1;
  // Later if available_ != expected_available_ then we can throw UnknownMissingException.
  saw_unk_ = false;
}

WordIndex ProbingVocabulary::Insert(const StringPiece &str) {
  uint64_t hashed = detail::HashForVocab(str);
  // Prevent unknown from going into the table.  
  if (hashed == kUnknownHash || hashed == kUnknownCapHash) {
    saw_unk_ = true;
    return 0;
  } else {
    lookup_.Insert(Lookup::Packing::Make(hashed, available_));
    return available_++;
  }
}

void ProbingVocabulary::FinishedLoading(ProbBackoff *reorder_vocab) {
  lookup_.FinishedInserting();
  SetSpecial(Index("<s>"), Index("</s>"), 0, available_);
}

void ProbingVocabulary::LoadedBinary() {
  lookup_.LoadedBinary();
  SetSpecial(Index("<s>"), Index("</s>"), 0, available_);
}

} // namespace lm
