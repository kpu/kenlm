#include "lm/vocab.hh"

#include "lm/enumerate_vocab.hh"
#include "lm/lm_exception.hh"
#include "lm/ngram_config.hh"
#include "lm/weights.hh"
#include "util/exception.hh"
#include "util/joint_sort.hh"
#include "util/murmur_hash.hh"
#include "util/probing_hash_table.hh"

#include <string>

namespace lm {
namespace ngram {

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

void ReadWords(std::size_t expected, int fd, EnumerateVocab *enumerate) {
  if (!enumerate) return;
  const std::size_t kBufSize = 16384;
  std::string buf;
  buf.reserve(kBufSize + 100);
  // <unk> was sent by the destructor.
  WordIndex index = 1;
  while (true) {
    ssize_t got = read(fd, &buf[0], kBufSize);
    if (got == -1) UTIL_THROW(util::ErrnoException, "Reading vocabulary words");
    if (got == 0) {
      if (index != expected) UTIL_THROW(FormatLoadException, "Binary file has " << index << " vocabulary words but " << expected << " were expected.");
      return;
    }
    buf.resize(got);
    while (buf[buf.size() - 1]) {
      char next_char;
      ssize_t ret = read(fd, &next_char, 1);
      if (ret == -1) UTIL_THROW(util::ErrnoException, "Reading vocabulary words");
      if (ret == 0) UTIL_THROW(FormatLoadException, "Missing null terminator on a vocab word.");
      buf.push_back(next_char);
    }
    // Ok now we have null terminated strings.  
    for (const char *i = buf.data(); i != buf.data() + buf.size();) {
      std::size_t length = strlen(i);
      enumerate->Add(index++, StringPiece(i, length));
      i += length + 1 /* null byte */;
    }
  }
}

} // namespace

SortedVocabulary::SortedVocabulary(EnumerateVocab *enumerate) : begin_(NULL), end_(NULL), enumerate_(enumerate) {}

std::size_t SortedVocabulary::Size(std::size_t entries, const Config &/*config*/) {
  // Lead with the number of entries.  
  return sizeof(uint64_t) + sizeof(Entry) * entries;
}

void SortedVocabulary::SetupMemory(void *start, std::size_t allocated, std::size_t entries, const Config &config) {
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
  SetSpecial(Index("<s>"), Index("</s>"), 0);
  // Save size.  
  *(reinterpret_cast<uint64_t*>(begin_) - 1) = end_ - begin_;
}

void SortedVocabulary::LoadedBinary(std::size_t expected_count, int fd) {
  end_ = begin_ + *(reinterpret_cast<const uint64_t*>(begin_) - 1);
  ReadWords(expected_count, fd, enumerate_);
  SetSpecial(Index("<s>"), Index("</s>"), 0);
}

ProbingVocabulary::ProbingVocabulary(EnumerateVocab *enumerate) : enumerate_(enumerate) {
  if (enumerate_) enumerate_->Add(0, "<unk>");
}

std::size_t ProbingVocabulary::Size(std::size_t entries, const Config &config) {
  return Lookup::Size(entries, config.probing_multiplier);
}

void ProbingVocabulary::SetupMemory(void *start, std::size_t allocated, std::size_t /*entries*/, const Config &config) {
  lookup_ = Lookup(start, allocated);
  available_ = 1;
  saw_unk_ = false;
}

WordIndex ProbingVocabulary::Insert(const StringPiece &str) {
  uint64_t hashed = detail::HashForVocab(str);
  // Prevent unknown from going into the table.  
  if (hashed == kUnknownHash || hashed == kUnknownCapHash) {
    saw_unk_ = true;
    return 0;
  } else {
    if (enumerate_) enumerate_->Add(available_, str);
    lookup_.Insert(Lookup::Packing::Make(hashed, available_));
    return available_++;
  }
}

void ProbingVocabulary::FinishedLoading(ProbBackoff * /*reorder_vocab*/) {
  lookup_.FinishedInserting();
  SetSpecial(Index("<s>"), Index("</s>"), 0);
}

void ProbingVocabulary::LoadedBinary(std::size_t expected_count, int fd) {
  lookup_.LoadedBinary();
  ReadWords(expected_count, fd, enumerate_);
  SetSpecial(Index("<s>"), Index("</s>"), 0);
}

} // namespace ngram
} // namespace lm
