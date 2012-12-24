#include "util/stream/multi_progress.hh"

// TODO: merge some functionality with the simple progress bar?
#include "util/ersatz_progress.hh"

#include <limits>

namespace util { namespace stream {

namespace {
const char kDisplayCharacters[] = "-+*#0123456789";

uint64_t Next(unsigned char stone, uint64_t complete) {
  return (static_cast<uint64_t>(stone + 1) * complete + MultiProgress::kWidth - 1) / MultiProgress::kWidth;
}

} // namespace

MultiProgress::MultiProgress(std::ostream *out, uint64_t complete) : out_(NULL) {
  Reset(out, complete);
}

void MultiProgress::Reset(std::ostream *out, uint64_t complete) {
  if (out_) *out_ << '\n';
  out_ = out;
  if (!out) {
    complete_ = std::numeric_limits<uint64_t>::max();
  } else if (!complete) {
    complete_ = 1;
  } else {
    complete_ = complete;
  }
  memset(display_, 0, sizeof(display_));
  character_handout_ = 0;
  if (out_) (*out_) << kProgressBanner;
}

WorkerProgress MultiProgress::Add() {
  if (!out_)
    return WorkerProgress(std::numeric_limits<uint64_t>::max(), *this, '\0');
  std::size_t character_index;
  {
    boost::unique_lock<boost::mutex> lock(mutex_);
    character_index = character_handout_++;
    if (character_handout_ == sizeof(kDisplayCharacters) - 1)
      character_handout_ = 0;
  }
  return WorkerProgress(Next(0, complete_), *this, kDisplayCharacters[character_index]);
}

MultiProgress::~MultiProgress() {
  if (out_) (*out_) << '\n';
}

void MultiProgress::Milestone(WorkerProgress &worker) {
  assert(out_);
  unsigned char stone = std::min(static_cast<uint64_t>(kWidth), worker.current_ * kWidth / complete_);
  for (char *i = &display_[worker.stone_]; i < &display_[stone]; ++i) {
    *i = worker.character_;
  }
  worker.next_ = Next(stone, complete_);
  worker.stone_ = stone;
  {
    boost::unique_lock<boost::mutex> lock(mutex_);
    (*out_) << '\r' << display_ << std::flush;
  }
}

}} // namespaces
