#ifndef BOUNDED_CONFIG__
#define BOUNDED_CONFIG__

#include <string>

namespace bounded {

struct Config {
  std::size_t total_memory;
  // Amount of free memory when a spill is triggered.  
  std::size_t threshold_memory;

  std::string temporary_template;
  // Block size for scroll.  This is a goal since it has to be a multiple of the element size.  
  std::size_t goal_block_size;

  // Size of blocks passed to write.  
  std::size_t write_block_size;

  // Number of blocks to read ahead.    
  std::size_t read_ahead_blocks;

  // How many files to merge at once 
  unsigned merge_arity;

  Config() :
    total_memory(1ULL << 31),
    threshold_memory(total_memory / 10),
    temporary_template("/tmp/bounded_XXXXXX"),
    goal_block_size(1ULL << 25),
    write_block_size(1ULL << 26),
    read_ahead_blocks(2),
    merge_arity(3) {}
};

} // namespace bounded

#endif // BOUNDED_CONFIG__
