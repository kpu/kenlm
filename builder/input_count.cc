#include "builder/input_count.hh"
#include "builder/ngram.hh"
#include "lm/lm_exception.hh"
#include "lm/word_index.hh"
#include "util/file.hh"
#include "util/murmur_hash.hh"

#include <stdint.h>

namespace lm {
namespace builder {

VocabHandout::VocabHandout(const char *name) : word_list_(util::FOpenOrThrow(name, "wb")) {
  Lookup("<unk>"); // Force 0
  Lookup("<s>"); // Force 1
}

WordIndex VocabHandout::Lookup(const StringPiece &word) {
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

} // namespace builder
} // namespace lm
