#ifndef LM_BUILDER_TIMER__
#define LM_BUILDER_TIMER__

#include <boost/version.hpp>

#if BOOST_VERSION >= 104800
#include <boost/timer/timer.hpp>
#define LM_TIMER(str) boost::timer::auto_cpu_timer timer(std::cerr, 1, (str))
#else
#define LM_TIMER(str) 
#endif

#endif // LM_BUILDER_TIMER__
