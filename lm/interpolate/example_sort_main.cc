#include "lm/interpolate/arpa_to_stream.hh"

#include "lm/builder/print.hh"
#include "lm/builder/sort.hh"
#include "lm/vocab.hh"
#include "util/file.hh"
#include "util/unistd.hh"


int main() {
  
  // TODO: Make these all command-line parameters
  const std::size_t ONE_GB = 1 << 30;
  const std::size_t SIXTY_FOUR_MB = 1 << 26;
  const std::size_t NUMBER_OF_BLOCKS = 2;
  
  // Vocab strings will be written to this file, forgotten, and reconstituted
  // later.  This saves memory.
  util::scoped_fd vocab_file(util::MakeTemp("/tmp/"));
  std::vector<uint64_t> counts;
  util::stream::Chains chains;
  {
    // Use consistent vocab ids across models.
    lm::ngram::GrowableVocab<lm::ngram::WriteUniqueWords> vocab(10, vocab_file.get());
    lm::interpolate::ARPAToStream reader(STDIN_FILENO, vocab);
    counts = reader.Counts();

    // Configure a chain for each order.  TODO: extract chain balance heuristics from lm/builder/pipeline.cc
    chains.Init(reader.Order());

    for (std::size_t i = 0; i < reader.Order(); ++i) {
      
      // The following call to chains.push_back() invokes the Chain constructor
      //     and appends the newly created Chain object to the chains array
      chains.push_back(util::stream::ChainConfig(lm::builder::NGram::TotalSize(i + 1), NUMBER_OF_BLOCKS, ONE_GB));
      
    }
    
    // The following call to the >> method of chains
    //    constructs a ChainPosition for each chain in chains using Chain::Add();
    //    that function begins with a call to Chain::Start()
    //    that allocates memory for the chain.
    //
    // After the following call to the >> method of chains, 
    //    a new thread will be running
    //    and will be executing the reader.Run() method
    //    to read through the body of the ARPA file from standard input.
    //
    // For each n-gram line in the ARPA file,
    //    the thread executing reader.Run() 
    //    will write the probability, the n-gram, and the backoff
    //    to the appropriate location in the appropriate chain 
    //    (for details, see the ReadNGram() method in read_arpa.hh).
    //
    // Normally >> copies then runs so inline >> works.  But here we want a ref.
    chains >> boost::ref(reader);
    

    util::stream::SortConfig sort_config;
    sort_config.temp_prefix  = "/tmp/";
    sort_config.buffer_size  = SIXTY_FOUR_MB;
    sort_config.total_memory = ONE_GB;
    
    // Parallel sorts across orders (though somewhat limited because ARPA files are not being read in parallel across orders)
    lm::builder::Sorts<lm::builder::SuffixOrder> sorts(reader.Order());
    for (std::size_t i = 0; i < reader.Order(); ++i) {

      // The following call to sorts.push_back() invokes the Sort constructor
      //     and appends the newly constructed Sort object to the sorts array.
      //
      // After the construction of the Sort object,
      //    two new threads will be running (each owned by the chains[i] object).
      //
      // The first new thread will execute BlockSorter.Run() to sort the n-gram entries of order (i+1)
      //    that were previously read into chains[i] by the ARPA input reader thread.
      //
      // The second new thread will execute WriteAndRecycle.Run() 
      //    to write each sorted block of data to disk as a temporary file.
      sorts.push_back(chains[i], sort_config, lm::builder::SuffixOrder(i + 1));

    }
    
    // Output to the same chains.
    for (std::size_t i = 0; i < reader.Order(); ++i) {
      
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
      //     and how much memory we can use to read from each block (sort_config.buffer_size)
      //     it may be the case that we have insufficient memory 
      //     to read sort_config.buffer_size of data from each block from disk.
      //
      // If this occurs, then it will be necessary to perform one or more rounds of merge sort on disk;
      //     doing so will reduce the number of blocks that we will eventually need to read from
      //     when performing the final round of merge sort in memory.
      //
      // So, the following call determines whether it is necessary
      //     to perform one or more rounds of merge sort on disk;
      //     if such on-disk merge sorting is required, such sorting is performed.
      //
      // Finally, the following method launches a thread that calls OwningMergingReader.Run()
      //     to perform the final round of merge sort in memory.
      //
      // Merge sort could have be invoked directly
      //     so that merge sort memory doesn't coexist with Chain memory.
      sorts[i].Output(chains[i]);
    }
    
    // sorts can go out of scope even though it's still writing to the chains.
    // note that vocab going out of scope flushes to vocab_file.
  }

  
  // Get the vocabulary mapping used for this ARPA file
  lm::builder::VocabReconstitute reconstitute(vocab_file.get());
  
  // After the following call to the << method of chains,
  //    a new thread will be running
  //    and will be executing the Run() method of PrintARPA
  //    to print the final sorted ARPA file to standard output.
  chains >> lm::builder::PrintARPA(reconstitute, counts, NULL, STDOUT_FILENO);
  
  // Joins all threads that chains owns, 
  //    and does a for loop over each chain object in chains,
  //    calling chain.Wait() on each such chain object
  chains.Wait(true);
  
}
