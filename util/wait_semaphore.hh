#ifndef UTIL_WAIT_SEMAPHORE__
#define UTIL_WAIT_SEMAPHORE__

// Wait on a semaphore retrying on EINTR

#include <boost/interprocess/sync/interprocess_semaphore.hpp>

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

} // namespace util

#endif // UTIL_WAIT_SEMAPHORE__
