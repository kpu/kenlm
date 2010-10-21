#ifndef LM_NGRAM_CONFIG__
#define LM_NGRAM_CONFIG__

/* Configuration for ngram model.  Separate header to reduce pollution. */

#include <iostream>

namespace lm { namespace ngram {

struct Config {
  // EFFECTIVE FOR BOTH ARPA AND BINARY READS 

  // Where to log messages including the progress bar.  Set to NULL for
  // silence.
  std::ostream *messages;



  // ONLY EFFECTIVE WHEN READING ARPA

  // What to do when <unk> isn't in the provided model. 
  typedef enum {THROW_UP, COMPLAIN, SILENT} UnknownMissing;
  UnknownMissing unknown_missing;

  // The probability to substitute for <unk> if it's missing from the model.  
  // No effect if the model has <unk> or unknown_missing == THROW_UP.
  float unknown_missing_prob;

  // Size multiplier for probing hash table.  Must be > 1.  Space is linear in
  // this.  Time is probing_multiplier / (probing_multiplier - 1).  No effect
  // for sorted variant.  
  // If you find yourself setting this to a low number, consider using the
  // Sorted version instead which has lower memory consumption.  
  float probing_multiplier;

  // Amount of memory to use for building.  The actual memory usage will be
  // higher since this just sets sort buffer size.  Only applies to trie
  // models.
  std::size_t building_memory;

  // Directory in which to place temporary files.  File naming is
  // deterministic.  Disk usage will be on the order to the ARPA size.  
  // Only used by trie models.  


  // While loading an ARPA file, also write out this binary format file.  Set
  // to NULL to disable.  
  const char *write_mmap;

  

  // ONLY EFFECTIVE WHEN READING BINARY
  bool prefault;



  // Defaults. 
  Config() :
    messages(&std::cerr),
    unknown_missing(COMPLAIN),
    unknown_missing_prob(0.0),
    probing_multiplier(1.5),
    write_mmap(NULL),
    prefault(false) {}
};

} /* namespace ngram */ } /* namespace lm */

#endif // LM_NGRAM_CONFIG__
