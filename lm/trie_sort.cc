#include "lm/trie_sort.hh"

#include "lm/config.hh"
#include "lm/lm_exception.hh"
#include "lm/read_arpa.hh"
#include "lm/vocab.hh"
#include "lm/weights.hh"
#include "lm/word_index.hh"
#include "util/file_piece.hh"
#include "util/mmap.hh"
#include "util/proxy_iterator.hh"
#include "util/sized_iterator.hh"

#include <algorithm>
#include <cstring>
#include <cstdio>
#include <deque>
#include <limits>
#include <vector>

namespace lm {
namespace ngram {
namespace trie {

const char *kContextSuffix = "_contexts";

FILE *OpenOrThrow(const char *name, const char *mode) {
  FILE *ret = fopen(name, mode);
  if (!ret) UTIL_THROW(util::ErrnoException, "Could not open " << name << " for " << mode);
  return ret;
}

void WriteOrThrow(FILE *to, const void *data, size_t size) {
  assert(size);
  if (1 != std::fwrite(data, size, 1, to)) UTIL_THROW(util::ErrnoException, "Short write; requested size " << size);
}

namespace {

typedef util::SizedIterator NGramIter;

// Proxy for an entry except there is some extra cruft between the entries.  This is used to sort (n-1)-grams using the same memory as the sorted n-grams.  
class PartialViewProxy {
  public:
    PartialViewProxy() : attention_size_(0), inner_() {}

    PartialViewProxy(void *ptr, std::size_t block_size, std::size_t attention_size) : attention_size_(attention_size), inner_(ptr, block_size) {}

    operator std::string() const {
      return std::string(reinterpret_cast<const char*>(inner_.Data()), attention_size_);
    }

    PartialViewProxy &operator=(const PartialViewProxy &from) {
      memcpy(inner_.Data(), from.inner_.Data(), attention_size_);
      return *this;
    }

    PartialViewProxy &operator=(const std::string &from) {
      memcpy(inner_.Data(), from.data(), attention_size_);
      return *this;
    }

    const void *Data() const { return inner_.Data(); }
    void *Data() { return inner_.Data(); }

  private:
    friend class util::ProxyIterator<PartialViewProxy>;

    typedef std::string value_type;

    const std::size_t attention_size_;

    typedef util::SizedInnerIterator InnerIterator;
    InnerIterator &Inner() { return inner_; }
    const InnerIterator &Inner() const { return inner_; } 
    InnerIterator inner_;
};

typedef util::ProxyIterator<PartialViewProxy> PartialIter;

std::string DiskFlush(const void *mem_begin, const void *mem_end, const std::string &file_prefix, std::size_t batch, unsigned char order) {
  std::stringstream assembled;
  assembled << file_prefix << static_cast<unsigned int>(order) << '_' << batch;
  std::string ret(assembled.str());
  util::scoped_fd out(util::CreateOrThrow(ret.c_str()));
  util::WriteOrThrow(out.get(), mem_begin, (uint8_t*)mem_end - (uint8_t*)mem_begin);
  return ret;
}

void WriteContextFile(uint8_t *begin, uint8_t *end, const std::string &ngram_file_name, std::size_t entry_size, unsigned char order) {
  const size_t context_size = sizeof(WordIndex) * (order - 1);
  // Sort just the contexts using the same memory.  
  PartialIter context_begin(PartialViewProxy(begin + sizeof(WordIndex), entry_size, context_size));
  PartialIter context_end(PartialViewProxy(end + sizeof(WordIndex), entry_size, context_size));

  std::sort(context_begin, context_end, util::SizedCompare<EntryCompare, PartialViewProxy>(EntryCompare(order - 1)));

  std::string name(ngram_file_name + kContextSuffix);
  util::scoped_FILE out(OpenOrThrow(name.c_str(), "w"));

  // Write out to file and uniqueify at the same time.  Could have used unique_copy if there was an appropriate OutputIterator.  
  if (context_begin == context_end) return;
  PartialIter i(context_begin);
  WriteOrThrow(out.get(), i->Data(), context_size);
  const void *previous = i->Data();
  ++i;
  for (; i != context_end; ++i) {
    if (memcmp(previous, i->Data(), context_size)) {
      WriteOrThrow(out.get(), i->Data(), context_size);
      previous = i->Data();
    }
  }
}

struct ThrowCombine {
  void operator()(std::size_t /*entry_size*/, const void * /*first*/, const void * /*second*/, FILE * /*out*/) const {
    UTIL_THROW(FormatLoadException, "Duplicate n-gram detected.");
  }
};

// Useful for context files that just contain records with no value.  
struct FirstCombine {
  void operator()(std::size_t entry_size, const void *first, const void * /*second*/, FILE *out) const {
    WriteOrThrow(out, first, entry_size);
  }
};

template <class Combine> void MergeSortedFiles(const std::string &first_name, const std::string &second_name, const std::string &out, std::size_t weights_size, unsigned char order, const Combine &combine = ThrowCombine()) {
  std::size_t entry_size = sizeof(WordIndex) * order + weights_size;
  RecordReader first, second;
  first.Init(first_name.c_str(), entry_size);
  util::RemoveOrThrow(first_name.c_str());
  second.Init(second_name.c_str(), entry_size);
  util::RemoveOrThrow(second_name.c_str());
  util::scoped_FILE out_file(OpenOrThrow(out.c_str(), "w"));
  EntryCompare less(order);
  while (first && second) {
    if (less(first.Data(), second.Data())) {
      WriteOrThrow(out_file.get(), first.Data(), entry_size);
      ++first;
    } else if (less(second.Data(), first.Data())) {
      WriteOrThrow(out_file.get(), second.Data(), entry_size);
      ++second;
    } else {
      combine(entry_size, first.Data(), second.Data(), out_file.get());
      ++first; ++second;
    }
  }
  for (RecordReader &remains = (first ? first : second); remains; ++remains) {
    WriteOrThrow(out_file.get(), remains.Data(), entry_size);
  }
}

void ConvertToSorted(util::FilePiece &f, const SortedVocabulary &vocab, const std::vector<uint64_t> &counts, util::scoped_memory &mem, const std::string &file_prefix, unsigned char order, PositiveProbWarn &warn) {
  ReadNGramHeader(f, order);
  const size_t count = counts[order - 1];
  // Size of weights.  Does it include backoff?  
  const size_t words_size = sizeof(WordIndex) * order;
  const size_t weights_size = sizeof(float) + ((order == counts.size()) ? 0 : sizeof(float));
  const size_t entry_size = words_size + weights_size;
  const size_t batch_size = std::min(count, mem.size() / entry_size);
  uint8_t *const begin = reinterpret_cast<uint8_t*>(mem.get());
  std::deque<std::string> files;
  for (std::size_t batch = 0, done = 0; done < count; ++batch) {
    uint8_t *out = begin;
    uint8_t *out_end = out + std::min(count - done, batch_size) * entry_size;
    if (order == counts.size()) {
      for (; out != out_end; out += entry_size) {
        ReadNGram(f, order, vocab, reinterpret_cast<WordIndex*>(out), *reinterpret_cast<Prob*>(out + words_size), warn);
      }
    } else {
      for (; out != out_end; out += entry_size) {
        ReadNGram(f, order, vocab, reinterpret_cast<WordIndex*>(out), *reinterpret_cast<ProbBackoff*>(out + words_size), warn);
      }
    }
    // Sort full records by full n-gram.  
    util::SizedProxy proxy_begin(begin, entry_size), proxy_end(out_end, entry_size);
    // parallel_sort uses too much RAM
    std::sort(NGramIter(proxy_begin), NGramIter(proxy_end), util::SizedCompare<EntryCompare>(EntryCompare(order)));
    files.push_back(DiskFlush(begin, out_end, file_prefix, batch, order));
    WriteContextFile(begin, out_end, files.back(), entry_size, order);

    done += (out_end - begin) / entry_size;
  }

  // All individual files created.  Merge them.  

  std::size_t merge_count = 0;
  while (files.size() > 1) {
    std::stringstream assembled;
    assembled << file_prefix << static_cast<unsigned int>(order) << "_merge_" << (merge_count++);
    files.push_back(assembled.str());
    MergeSortedFiles(files[0], files[1], files.back(), weights_size, order, ThrowCombine());
    MergeSortedFiles(files[0] + kContextSuffix, files[1] + kContextSuffix, files.back() + kContextSuffix, 0, order - 1, FirstCombine());
    files.pop_front();
    files.pop_front();
  }
  if (!files.empty()) {
    std::stringstream assembled;
    assembled << file_prefix << static_cast<unsigned int>(order) << "_merged";
    std::string merged_name(assembled.str());
    if (std::rename(files[0].c_str(), merged_name.c_str())) UTIL_THROW(util::ErrnoException, "Could not rename " << files[0].c_str() << " to " << merged_name.c_str());
    std::string context_name = files[0] + kContextSuffix;
    merged_name += kContextSuffix;
    if (std::rename(context_name.c_str(), merged_name.c_str())) UTIL_THROW(util::ErrnoException, "Could not rename " << context_name << " to " << merged_name.c_str());
  }
}

} // namespace

void RecordReader::Init(const std::string &name, std::size_t entry_size) {
  file_.reset(OpenOrThrow(name.c_str(), "r+"));
  data_.reset(malloc(entry_size));
  UTIL_THROW_IF(!data_.get(), util::ErrnoException, "Failed to malloc read buffer");
  remains_ = true;
  entry_size_ = entry_size;
  ++*this;
}

void RecordReader::Overwrite(const void *start, std::size_t amount) {
  long internal = (uint8_t*)start - (uint8_t*)data_.get();
  UTIL_THROW_IF(fseek(file_.get(), internal - entry_size_, SEEK_CUR), util::ErrnoException, "Couldn't seek backwards for revision");
  WriteOrThrow(file_.get(), start, amount);
  long forward = entry_size_ - internal - amount;
  if (forward) UTIL_THROW_IF(fseek(file_.get(), forward, SEEK_CUR), util::ErrnoException, "Couldn't seek forwards past revision");
}

void ARPAToSortedFiles(const Config &config, util::FilePiece &f, std::vector<uint64_t> &counts, size_t buffer, const std::string &file_prefix, SortedVocabulary &vocab) {
  PositiveProbWarn warn(config.positive_log_probability);
  {
    std::string unigram_name = file_prefix + "unigrams";
    util::scoped_fd unigram_file;
    // In case <unk> appears.  
    size_t file_out = (counts[0] + 1) * sizeof(ProbBackoff);
    util::scoped_mmap unigram_mmap(util::MapZeroedWrite(unigram_name.c_str(), file_out, unigram_file), file_out);
    Read1Grams(f, counts[0], vocab, reinterpret_cast<ProbBackoff*>(unigram_mmap.get()), warn);
    CheckSpecials(config, vocab);
    if (!vocab.SawUnk()) ++counts[0];
  }

  // Only use as much buffer as we need.  
  size_t buffer_use = 0;
  for (unsigned int order = 2; order < counts.size(); ++order) {
    buffer_use = std::max<size_t>(buffer_use, static_cast<size_t>((sizeof(WordIndex) * order + 2 * sizeof(float)) * counts[order - 1]));
  }
  buffer_use = std::max<size_t>(buffer_use, static_cast<size_t>((sizeof(WordIndex) * counts.size() + sizeof(float)) * counts.back()));
  buffer = std::min<size_t>(buffer, buffer_use);

  util::scoped_memory mem;
  mem.reset(malloc(buffer), buffer, util::scoped_memory::MALLOC_ALLOCATED);
  if (!mem.get()) UTIL_THROW(util::ErrnoException, "malloc failed for sort buffer size " << buffer);

  for (unsigned char order = 2; order <= counts.size(); ++order) {
    ConvertToSorted(f, vocab, counts, mem, file_prefix, order, warn);
  }
  ReadEnd(f);
}

} // namespace trie
} // namespace ngram
} // namespace lm
