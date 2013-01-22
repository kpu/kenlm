#ifndef UTIL_STREAM_TIMER__
#define UTIL_STREAM_TIMER__

// Sorry Jon, this was adding library dependencies in Moses and people complained.

/*#include <boost/version.hpp>

#if BOOST_VERSION >= 104800
#include <boost/timer/timer.hpp>
#define UTIL_TIMER(str) boost::timer::auto_cpu_timer timer(std::cerr, 1, (str))
#else
//#warning Using Boost older than 1.48. Timing information will not be available.*/
#define UTIL_TIMER(str) 
//#endif

#endif // UTIL_STREAM_TIMER__
