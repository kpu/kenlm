#ifndef UTIL_ERSATZ_PROGRESS__
#define UTIL_ERSATZ_PROGRESS__

#include <iosfwd>
#include <string>

// Ersatz version of boost::progress so core language model doesn't depend on
// boost.  Also adds option to print nothing.  

namespace util {
class ErsatzProgress {
  public:
    // No output.  
    ErsatzProgress();

    // Null means no output.  The null value is useful for passing along the ostream pointer from another caller.   
    ErsatzProgress(std::ostream *to, const std::string &message, std::size_t complete);

    ~ErsatzProgress();

    ErsatzProgress &operator++() {
      if (++current_ >= next_) Milestone();
      return *this;
    }

    ErsatzProgress &operator+=(std::size_t amount) {
      if ((current_ += amount) >= next_) Milestone();
      return *this;
    }

    void Set(std::size_t to) {
      if ((current_ = to) >= next_) Milestone();
      Milestone();
    }

    void Finished() {
      Set(complete_);
    }

  private:
    void Milestone();

    std::size_t current_, next_, complete_;
    unsigned char stones_written_;
    std::ostream *out_;

    // noncopyable
    ErsatzProgress(const ErsatzProgress &other);
    ErsatzProgress &operator=(const ErsatzProgress &other);
};

} // namespace util

#endif // UTIL_ERSATZ_PROGRESS__
