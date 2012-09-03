#include "bounded/input.hh"

#include "bounded/chunk.hh"

#include <algorithm>

namespace bounded {

Input::~Input() {}

Input::Input(Chunk &chunk) : chunk_(chunk) {}

MMapInput::MMapInput(Manager &manager, Chunk &chunk) : Input(chunk), manager_(manager) {}

std::size_t MMapInput::Take() {
  manager_.Grow(chunk_.BlockSize());
  chunk_.GrowEnd(chunk_.BlockSize());
  return chunk_.BlockSize();
}

ReadAhead::ReadAhead(const Config &config, Chunk &chunk, Input *inner) :
  Input(chunk),
  inner_(inner),
  available_(0),
  finished_(false),
  read_ahead_size_(config.read_ahead_blocks * chunk_.BlockSize())
  ahead_(&ReadAhead::Reader, this) {}

ReadAhead::~ReadAhead() {
  ahead_.interrupt();
  ahead_.join();
}

std::size_t ReadAhead::Take() {
  {
    boost::unique_lock<boost::mutex> lock(update_mutex_);
    while (!available_) {
      if (finished_) return 0;
      update_cond_.wait(lock);
    }
    std::size_t ret = available_;
    available_ = 0;
  }
  difference_.notify_all();
  return ret;
}

void ReadAhead::Reader() {
  while (true) {
    boost::unique_lock<boost::mutex> lock(update_mutex_);
    while (available_ >= read_ahead_size_) {
      update_cond_.wait(lock);
    }
    std::size_t ret = inner_->Take();
    if (!ret) {
      finished_ = true;
      lock.unlock();
      update_cond_.notify_all();
      return;
    }
    available_ += ret;
    lock.unlock();
    update_cond_.notify_all();
  }
}

MemoryInput::MemoryInput(Manager &manager, Chunk &chunk) :
  Input(chunk),
  SpillClient(manager),
  spillable_(reinterpret_cast<uint8_t*>(chunk.End()) - reinterpret_cast<uint8_t*>(chunk.Begin())),
  next_start_(0) {
  manager_.AddSpiller(*this);
}

MemoryInput::~MemoryInput() {
  manager_.RemoveSpiller(*this);
}

namespace {
void ReadOrThrow(int fd, void *to_void, std::size_t amount, off_t offset) {
  ssize_t ret;
  uint8_t *to = reinterpret_cast<uint8_t*>(to_void);
  while (amount) {
    UTIL_THROW_IF(-1 == (ret = pread(fd, to, amount, offset)), util::ErrnoException, "Failed to read");
    UTIL_THROW_IF(!ret, util::EndOfFileException, " during read with " << amount << " bytes to go");
    amount -= ret;
    to += ret;
  }
}
void WriteOrThrow(int fd, const void *from_void, std::size_t amount, off_t offset) {
  ssize_t ret;
  const uint8_t *from = reinterpret_cast<const uint8_t*>(from_void);
  while (amount) {
    UTIL_THROW_IF(-1 == (ret = pwrite(fd, from, amount, offset)), util::ErrnoException, "Failed to write");
    amount -= ret;
    from += ret;
  }
}
} // namespace

std::size_t MemoryInput::Take() {
  {
    boost::lock_guard<boost::mutex> lock(spillable_mutex_);
    if (spillable_) {
      std::size_t ret = std::min(spillable_, chunk_.BlockSize());
      spillable_ -= ret;
      return ret;
    }
  }

  // spillable_ = 0 but the spiller could still be running in which case we wait for it to finish.  
  {
    boost::lock_guard<boost::mutex> lock(file_mutex_);
  }
  if (dumps_.empty()) return 0;
  Record &top = dumps_.top();
  std::size_t amount = std::min(chunk_.BlockSize(), top.size);
  // Since spillable_ = 0 and spillable_mutex_ is free, Spill will return if the manager calls it.  
  manager_.Grow(amount);
  chunk_.GrowEnd(amount);
  ReadOrThrow(file_.get(), reinterpret_cast<uint8_t*>(chunk_.End()) - amount, amount, top.off);
  top.size -= amount;
  top.off += amount;
  if (!top.size) {
    dumps_.pop();
    if (dumps_.empty()) {
      file_.reset();
    } else {
      UTIL_THROW_IF(ftruncate(file_.get(), dumps_.top().off + dumps_.top().size), util::ErrnoException, "Failed to ftruncate a temporary file.");
    }
  }
  assert(amount);
  return amount;
}

std::size_t MemoryInput::SpillableSize() {
  boost::lock_guard<boost::mutex> lock(spillable_mutex_);
  return spillable_;
}

void MemoryInput::Spill() {
  boost::unique_lock<boost::mutex> file_lock(file_mutex_, boost::defer_lock_t);
  {
    boost::lock_guard<boost::mutex> lock(spillable_mutex_);
    if (!spillable_) return;
    std::size_t amount = std::min<std::size_t>(spillable_, chunk_.BlockSize());
    spillable_ -= amount;
    file_lock.lock();
  }

  Record record;
  record.off = next_start_;
  record.size = amount;
  if (!file_) file_.reset(OpenTemp(GetConfig()));
  WriteOrThrow(file_.get(), reinterpret_cast<uint8_t*>(chunk_.End()) - amount, amount, record.off);
  dumps_.push(record);
  chunk_.ShrinkEnd(amount);
  manager_.Shrink(amount);
  next_start_ += amount;
}

} // namespace bounded
