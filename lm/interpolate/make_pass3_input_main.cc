#include "lm/builder/sort.hh"
#include "lm/builder/print.hh"
#include "lm/builder/model_buffer.hh"

#if defined(_WIN32) || defined(_WIN64)

// Windows doesn't define <unistd.h>
//
// So we define what we need here instead:
//
#define STDIN_FILENO = 0
#define STDOUT_FILENO = 1
#else // Huzzah for POSIX!
#include <unistd.h>
#endif

/*
 * This is a simple example program that takes in intermediate
 * suffix-sorted ngram files and context sorts each using the streaming
 * framework.
 */
int main() {
  // TODO: Make these all command-line parameters
  const std::size_t ONE_GB = 1 << 30;
  const std::size_t SIXTY_FOUR_MB = 1 << 26;
  const std::size_t NUMBER_OF_BLOCKS = 2;
  const std::string FILE_NAME = "ngrams";
  const std::string CONTEXT_SORTED_FILENAME = "csorted-ngrams";

  // This will be used to read in the binary intermediate files. There is
  // one file per order (e.g. ngrams.1, ngrams.2, ...)
  lm::builder::ModelBuffer buffer(FILE_NAME);

  // Create a separate chain for each ngram order
  util::stream::Chains chains(buffer.Order());
  for (std::size_t i = 0; i < buffer.Order(); ++i) {
    chains.push_back(util::stream::ChainConfig(
        lm::builder::NGram::TotalSize(i + 1), NUMBER_OF_BLOCKS, ONE_GB));
  }

  // This sets the input for each of the ngram order chains to the
  // appropriate file
  buffer.Source(chains);

  util::stream::SortConfig sort_cfg;
  sort_cfg.temp_prefix = "/tmp/";
  sort_cfg.buffer_size = SIXTY_FOUR_MB;
  sort_cfg.total_memory = ONE_GB;

  // This will parallel merge sort the individual order files, putting
  // them in context-order instead of suffix-order.
  //
  // Two new threads will be running, each owned by the chains[i] object.
  // - The first executes BlockSorter.Run() to sort the n-gram entries
  // - The second executes WriteAndRecycle.Run() to write each sorted
  //   block to disk as a temporary file
  lm::builder::Sorts<lm::builder::ContextOrder> sorts(chains.size());
  for (std::size_t i = 0; i < chains.size(); ++i) {
    sorts.push_back(chains[i], sort_cfg, lm::builder::ContextOrder(i + 1));
  }

  // Set the sort output to be on the same chain
  for (std::size_t i = 0; i < chains.size(); ++i) {
    // The following call to Chain::Wait()
    //     joins the threads owned by chains[i].
    //
    // As such the following call won't return
    //     until all threads owned by chains[i] have completed.
    //
    // The following call also resets chain[i]
    //     so that it can be reused
    //     (including free'ing the memory previously used by the chain)
    chains[i].Wait();

    // In an ideal world (without memory restrictions)
    //     we could merge all of the previously sorted blocks
    //     by reading them all completely into memory
    //     and then running merge sort over them.
    //
    // In the real world, we have memory restrictions;
    //     depending on how many blocks we have,
    //     and how much memory we can use to read from each block
    //     (sort_config.buffer_size)
    //     it may be the case that we have insufficient memory
    //     to read sort_config.buffer_size of data from each block from disk.
    //
    // If this occurs, then it will be necessary to perform one or more rounds
    // of merge sort on disk;
    //     doing so will reduce the number of blocks that we will eventually
    //     need to read from
    //     when performing the final round of merge sort in memory.
    //
    // So, the following call determines whether it is necessary
    //     to perform one or more rounds of merge sort on disk;
    //     if such on-disk merge sorting is required, such sorting is performed.
    //
    // Finally, the following method launches a thread that calls
    // OwningMergingReader.Run()
    //     to perform the final round of merge sort in memory.
    //
    // Merge sort could have be invoked directly
    //     so that merge sort memory doesn't coexist with Chain memory.
    sorts[i].Output(chains[i]);
  }

  // Create another model buffer for our output on e.g. csorted-ngrams.1,
  // csorted-ngrams.2, ...
  lm::builder::ModelBuffer output_buf(CONTEXT_SORTED_FILENAME, true, false);
  output_buf.Sink(chains);

  // Joins all threads that chains owns,
  //    and does a for loop over each chain object in chains,
  //    calling chain.Wait() on each such chain object
  chains.Wait(true);

  return 0;
}
