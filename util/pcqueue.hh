#ifndef UTIL_PCQUEUE__
#define UTIL_PCQUEUE__

#include <boost/interprocess/sync/interprocess_semaphore.hpp>
#include <boost/scoped_array.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/utility.hpp>

#include <errno.h>

namespace util {

inline void WaitSemaphore (boost::interprocess::interprocess_semaphore &on) {
  while (1) {
    try {
      on.wait();
      break;
    }
    catch (boost::interprocess::interprocess_exception &e) {
      if (e.get_native_error() != EINTR) throw;
    }
  }
}

/* Producer consumer queue safe for multiple producers and multiple consumers.
 * T must be default constructable and have operator=.  
 * The value is copied twice for Consume(T &out) or three times for Consume(),
 * so larger objects should be passed via pointer.
 * Strong exception guarantee if operator= throws.  Undefined if semaphores throw.  
 */
template <class T> class PCQueue : boost::noncopyable {
 public:
  explicit PCQueue(size_t size)
   : empty_(size), used_(0),
     storage_(new T[size]),
     end_(storage_.get() + size),
     produce_at_(storage_.get()),
     consume_at_(storage_.get()) {}

  // Add a value to the queue.
  void Produce(const T &val) {
    WaitSemaphore(empty_);
    {
      boost::unique_lock<boost::mutex> produce_lock(produce_at_mutex_);
      try {
        *produce_at_ = val;
      }
      catch (...) {
        empty_.post();
        throw;
      }
      if (++produce_at_ == end_) produce_at_ = storage_.get();
    }
    used_.post();
  }

  // Consume a value, assigning it to out.
  T& Consume(T &out) {
    WaitSemaphore(used_);
    {
      boost::unique_lock<boost::mutex> consume_lock(consume_at_mutex_);
      try {
        out = *consume_at_;
      }
      catch (...) {
        used_.post();
        throw;
      }
      if (++consume_at_ == end_) consume_at_ = storage_.get();
    }
    empty_.post();
    return out;
  }

  // Convenience version of Consume that copies the value to return.
  // The other version is faster.
  T Consume() {
    T ret;
    Consume(ret);
    return ret;
  }
   
 private:
  // Number of empty spaces in storage_.
  boost::interprocess::interprocess_semaphore empty_;
  // Number of occupied spaces in storage_.
  boost::interprocess::interprocess_semaphore used_;

  boost::scoped_array<T> storage_;

  T *const end_;

  // Index for next write in storage_.
  T *produce_at_;
  boost::mutex produce_at_mutex_;

  // Index for next read from storage_.
  T *consume_at_;
  boost::mutex consume_at_mutex_;

};

} // namespace util

#endif // UTIL_PCQUEUE__
