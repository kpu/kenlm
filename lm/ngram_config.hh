#ifndef LM_NGRAM_CONFIG__
#define LM_NGRAM_CONFIG__

/* Configuration for ngram model.  Separate header to reduce pollution. */

#include <iostream>

namespace lm { namespace ngram {

struct Config {
  // What to do when <unk> isn't in the provided model. 
  // No effect on mmap loads.  
  typedef enum {THROW_UP, COMPLAIN, SILENT} UnknownMissing;
  UnknownMissing unknown_missing;

  // The probability to substitute for <unk> if it's missing from the model.  
  // No effect if the model has <unk> or unknown_missing == THROW_UP.
  // No effect on mmap loads.  
  float unknown_missing_prob;

  // Where to log messages including the progress bar.  Set to NULL for
  // silence.
  std::ostream *messages;

  // Size multiplier for probing hash table.  Must be > 1.  Space is linear in
  // this.  Time is probing_multiplier / (probing_multiplier - 1).  No effect
  // for sorted variant.  
  // If you find yourself setting this to a low number, consider using the
  // Sorted version instead which has lower memory consumption.  
  // Currently impacts mmap loads.  
  float probing_multiplier;

  // While loading an ARPA file, also write out this binary format file.  Only
  // effective if we're reading an ARPA.  Set to NULL to disable.  
  const char *write_mmap;

  // Prefault the giant mmap?  
  bool prefault;

  // Defaults.  This has an implicit constructor.  
  Config() :
    unknown_missing(COMPLAIN),
    unknown_missing_prob(0.0),
    messages(&std::cerr),
    probing_multiplier(1.5),
    write_mmap(NULL),
    prefault(false) {}
};

} /* namespace ngram */ } /* namespace lm */

#endif // LM_NGRAM_CONFIG__
