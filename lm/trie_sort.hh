#ifndef LM_TRIE_SORT__
#define LM_TRIE_SORT__

#include "lm/word_index.hh"

#include "util/file.hh"
#include "util/scoped.hh"

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include <inttypes.h>

namespace util { class FilePiece; }

// Step of trie builder: create sorted files.  
namespace lm {
namespace ngram {
class SortedVocabulary;
class Config;

namespace trie {

extern const char *kContextSuffix;
FILE *OpenOrThrow(const char *name, const char *mode);
void WriteOrThrow(FILE *to, const void *data, size_t size);

class EntryCompare : public std::binary_function<const void*, const void*, bool> {
  public:
    explicit EntryCompare(unsigned char order) : order_(order) {}

    bool operator()(const void *first_void, const void *second_void) const {
      const WordIndex *first = static_cast<const WordIndex*>(first_void);
      const WordIndex *second = static_cast<const WordIndex*>(second_void);
      const WordIndex *end = first + order_;
      for (; first != end; ++first, ++second) {
        if (*first < *second) return true;
        if (*first > *second) return false;
      }
      return false;
    }
  private:
    unsigned char order_;
};

class RecordReader {
  public:
    RecordReader() : remains_(true) {}

    void Init(const std::string &name, std::size_t entry_size);

    void *Data() { return data_.get(); }
    const void *Data() const { return data_.get(); }

    RecordReader &operator++() {
      std::size_t ret = fread(data_.get(), entry_size_, 1, file_.get());
      if (!ret) {
        UTIL_THROW_IF(!feof(file_.get()), util::ErrnoException, "Error reading temporary file");
        remains_ = false;
      }
      return *this;
    }

    operator bool() const { return remains_; }

    void Rewind() {
      rewind(file_.get());
      remains_ = true;
      ++*this;
    }

    std::size_t EntrySize() const { return entry_size_; }

    void Overwrite(const void *start, std::size_t amount);

  private:
    util::scoped_malloc data_;

    bool remains_;

    std::size_t entry_size_;

    util::scoped_FILE file_;
};

void ARPAToSortedFiles(const Config &config, util::FilePiece &f, std::vector<uint64_t> &counts, size_t buffer, const std::string &file_prefix, SortedVocabulary &vocab);

} // namespace trie
} // namespace ngram
} // namespace lm

#endif // LM_TRIE_SORT__
