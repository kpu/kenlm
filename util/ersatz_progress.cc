#include "util/ersatz_progress.hh"

#include <algorithm>
#include <ostream>
#include <limits>
#include <string>

namespace util {

namespace { const unsigned char kWidth = 100; }

ErsatzProgress::ErsatzProgress() : current_(0), next_(std::numeric_limits<std::size_t>::max()), complete_(next_), out_(NULL) {}

ErsatzProgress::~ErsatzProgress() {
  if (!out_) return;
  Finished();
}

ErsatzProgress::ErsatzProgress(std::ostream *to, const std::string &message, std::size_t complete) 
  : current_(0), next_(complete / kWidth), complete_(complete), stones_written_(0), out_(to) {
  if (!out_) {
    next_ = std::numeric_limits<std::size_t>::max();
    return;
  }
  *out_ << message << "\n----5---10---15---20---25---30---35---40---45---50---55---60---65---70---75---80---85---90---95--100\n";
}

void ErsatzProgress::Milestone() {
  if (!out_) { current_ = 0; return; }
  if (!complete_) return;
  unsigned char stone = std::min(static_cast<std::size_t>(kWidth), (current_ * kWidth) / complete_);

  for (; stones_written_ < stone; ++stones_written_) {
    (*out_) << '*';
  }
  if (stone == kWidth) {
    (*out_) << std::endl;
    next_ = std::numeric_limits<std::size_t>::max();
    out_ = NULL;
  } else {
    next_ = std::max(next_, (stone * complete_) / kWidth);
  }
}

} // namespace util
