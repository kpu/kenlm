#include "lm/interpolate/arpa_to_stream.hh"

#include "lm/builder/print.hh"
#include "lm/builder/sort.hh"
#include "lm/vocab.hh"
#include "util/file.hh"

#include <unistd.h>

int main() {
  const std::size_t ONE_GB = 1 << 30;
  const std::size_t SIXTY_FOUR_MB = 1 << 26;
  
  util::scoped_fd vocab_file(util::MakeTemp("/tmp/"));
  std::vector<uint64_t> counts;
  util::stream::Chains chains;
  {
    // Use consistent vocab ids across models.
    lm::ngram::GrowableVocab vocab(10, vocab_file.get());
    lm::interpolate::ARPAToStream reader(STDIN_FILENO, vocab);
    counts = reader.Counts();

    // Configure a chain for each order.  TODO: extract chain balance heuristics from lm/builder/pipeline.cc
    chains.Init(reader.Order());

    for (std::size_t i = 0; i < reader.Order(); ++i) {
      
      // The following call to chains.push_back() invokes the Chain constructor
      //     and appends the newly created Chain object to the chains array
      chains.push_back(util::stream::ChainConfig(lm::builder::NGram::TotalSize(i + 1), 2, ONE_GB));
      
    }
    
    
    // After the following call to the << method of chains, 
    //    a new thread will be running
    //    and will be executing the reader.Run() method
    //    to read through the body of the ARPA file from standard input
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
      // The first new thread will sort the n-gram entries of order (i+1)
      //    that were previously read into chains[i] by the ARPA input reader thread.
      //
      // The second new thread will write temporary data to disk.
      sorts.push_back(chains[i], sort_config, lm::builder::SuffixOrder(i + 1));

    }
    
    // Output to the same chains.
    for (std::size_t i = 0; i < reader.Order(); ++i) {
      
      // TODO: Describe what this call does
      chains[i].Wait();
      
      // TODO: Describe what this call does
      sorts[i].Output(chains[i]);
    }
    
    // sorts can go out of scope even though it's still writing to the chains.
    // note that vocab going out of scope flushes to vocab_file.
  }

  
  // Get the vocabulary mapping used for this ARPA file
  lm::builder::VocabReconstitute reconstitute(vocab_file.get());
  
  // After the following call to the << method of chains,
  //    a new thread will be running
  //    and will be executing the run() method of PrintARPA
  //    to print the final sorted ARPA file to standard output.
  chains >> lm::builder::PrintARPA(reconstitute, counts, NULL, STDOUT_FILENO);
  
  // TODO: Describe what this call does
  chains.Wait(true);
}
