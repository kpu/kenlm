#include "bounded/sort.hh"

#include "bounded/file.hh"

#include <unistd.h>

namespace bounded {
namespace detail {

Input *SortBase::FinishBackend() {
  std::size_t merged = RunSort(chunk_.End());
  std::size_t amount = (uint8_t*)chunk_.End() - (uint8_t*)chunk_.Begin();
  if (amount != merged) {
    chunk_.ShrinkEnd(amount - merged);
    manager_.Shrink(amount - merged);
  }
  if (queue_.empty()) {
    // No files.  
    return new ReadAhead(GetConfig(), chunk_, new MemoryInput(manager_, chunk_));
  }

  while (queue_.size() > 1) {
    // TODO: merging
    abort();
  }
  return new ReadAhead(GetConfig(), chunk_, new MergeInput(compare_, combine_, manager_, chunk_, queue_.top().release()));
}

SortBase::SortBase(Manager &manager, Chunk &chunk) : KeepOutput(manager, chunk) {}

SortBase::~SortBase() {}

void SortBase::DumpAndShrink(std::size_t amount) {
  void *end = static_cast<uint8_t*>(chunk_.Begin()) + amount;
  std::size_t merged = RunSort(end);
  boost::shared_ptr<File> f(new File(GetConfig(), merged));
  AppendAndRelease(chunk_, merged, manager_, f->get());
  if (merged != amount) {
    chunk_.ShrinkBegin(amount - merged);
    manager_.Shrink(amount - merged);
  }
  queue_.push(f);
}

SortBase::File::File(const Config &config, std::size_t size) : f_(OpenTemp(config)), size_(size) {}

} // namespace detail
} // namespace bounded
