#include "lm/interpolate/merge_vocab.hh"

#include "lm/interpolate/universal_vocab.hh"
#include "lm/lm_exception.hh"
#include "lm/vocab.hh"
#include "util/file_piece.hh"

#include <queue>
#include <string>
#include <iostream>
#include <vector>

namespace lm {
namespace interpolate {
namespace {

class VocabFileReader {
  public:
    explicit VocabFileReader(const int fd, size_t model_num, uint64_t offset = 0);

    VocabFileReader &operator++();
    operator bool() const { return !eof_; }
    uint64_t operator*() const { return Value(); }

    uint64_t Value(void) const { return hash_value_; }
    size_t ModelNum(void) const { return model_num_; }
    WordIndex CurrentIndex(void) const { return current_index_; }

  private:
    uint64_t hash_value_;
    WordIndex current_index_;
    bool eof_;
    size_t model_num_;
    util::FilePiece file_piece_;
};

VocabFileReader::VocabFileReader(const int fd, const size_t model_num, uint64_t offset) :
  hash_value_(0),
  current_index_(0),
  eof_(false),
  model_num_(model_num),
  file_piece_(fd)
{
  StringPiece vocab_elem = file_piece_.ReadLine('\0');
  UTIL_THROW_IF(vocab_elem != "<unk>",
                FormatLoadException,
                "Vocabulary words are in the wrong place.");
  // setup to initial value
  ++*this;
}

VocabFileReader &VocabFileReader::operator++() {
  StringPiece vocab_elem;
  try {
    vocab_elem = file_piece_.ReadLine('\0');
  } catch(util::EndOfFileException &e) {
    eof_ = true;
    return *this;
  }
  uint64_t prev_hash_value = hash_value_;
  hash_value_ = ngram::detail::HashForVocab(vocab_elem.data(), vocab_elem.size());

  // hash values should be monotonically increasing
  UTIL_THROW_IF(hash_value_ < prev_hash_value, FormatLoadException,
                ": word index not monotonically increasing."
                << " model_num: " << model_num_
                << " prev hash: " << prev_hash_value
                << " new hash: " << hash_value_);

  ++current_index_;
  return *this;
}

class CompareFiles {
public:
  bool operator()(const VocabFileReader* x,
                  const VocabFileReader* y)
  { return x->Value()> y->Value(); }
};

class Readers : public util::FixedArray<VocabFileReader> {
  public:
    Readers(std::size_t number) : util::FixedArray<VocabFileReader>(number) {}
    void push_back(int fd, std::size_t i) {
      new(end()) VocabFileReader(fd, i);
      Constructed();
    }
};

} // namespace

void MergeVocabIndex(util::FixedArray<util::scoped_fd> &files, UniversalVocab &vocab) {
  typedef std::priority_queue<VocabFileReader*, std::vector<VocabFileReader*>, CompareFiles> HeapType;
  HeapType heap;
  Readers readers(files.size());
  for (size_t i = 0; i < files.size(); ++i) {
    readers.push_back(files[i].release(), i);
    heap.push(&readers.back());
    // initialize first index to 0 for <unk>
    vocab.InsertUniversalIdx(i, 0, 0);
  }

  uint64_t prev_hash_value = 0;
  // global_index starts with <unk> which is 0
  WordIndex global_index = 0;

  while (!heap.empty()) {
    VocabFileReader* top_vocab_file = heap.top();
    if (top_vocab_file->Value() != prev_hash_value) {
      global_index++;
    }
    vocab.InsertUniversalIdx(top_vocab_file->ModelNum(),
        top_vocab_file->CurrentIndex(),
        global_index);

    prev_hash_value = top_vocab_file->Value();

    heap.pop();
    if (++(*top_vocab_file)) {
      heap.push(top_vocab_file);
    }
  }
}

} // namespace interpolate
} // namespace lm

