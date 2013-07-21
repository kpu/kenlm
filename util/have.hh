/* Optional packages.  You might want to integrate this with your build system e.g. config.h from ./configure. */
#ifndef UTIL_HAVE__
#define UTIL_HAVE__

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_ICU
//#define HAVE_ICU
#endif

#ifndef KENLM_MAX_ORDER
#error Implementing your own build system?  See README.md for the macros you have to define.
#endif

#endif // UTIL_HAVE__
