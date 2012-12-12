#ifndef UTIL_STREAM_CONFIG__
#define UTIL_STREAM_CONFIG__

#include <cstddef>
#include <string>

namespace util { namespace stream {

struct ChainConfig {
  std::size_t entry_size;
  // Chain's constructor will make thisa multiple of entry_size. 
  std::size_t block_size;
  std::size_t block_count;
  std::size_t queue_length;
};

struct SortConfig {
  std::string temp_prefix;
  // Number of readers to merge at once.  
  std::size_t arity;
  // Shared across all arity readers.  
  std::size_t total_read_buffer;

  // Same thing but for the lazy read.
  std::size_t lazy_arity;
  std::size_t lazy_total_read_buffer;

  // Configuration for the chain from reader to file writer.
  ChainConfig chain;
};

}} // namespaces
#endif // UTIL_STREAM_CONFIG__
