/* People have been abusing assert by assuming it will always execute.  To
 * rememdy the situation, asserts were replaced with CHECK.  These should then
 * be manually replaced with assert (when used correctly) or UTIL_THROW (for
 * runtime checks).  
 */
#ifndef UTIL_CHECK__
#define UTIL_CHECK__

#include <stdlib.h>
#include <iostream>

#include <cassert>

#define CHECK(Condition) do { \
  if (!(Condition)) { \
    std::cerr << "Check " << #Condition << " failed in " << __FILE__ << ":" << __LINE__ << std::endl; \
    abort(); \
  } \
} while (0) // swallow ;

#endif // UTIL_CHECK__
