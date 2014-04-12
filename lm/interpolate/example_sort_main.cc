#include "lm/interpolate/arpa_to_stream.hh"

#include "lm/builder/print.hh"
#include "lm/builder/sort.hh"
#include "lm/vocab.hh"
#include "util/file.hh"

int main() {
  util::scoped_fd vocab_file(util::MakeTemp("/tmp/"));
  std::vector<uint64_t> counts;
  util::stream::Chains chains;
  {
    // Use consistent vocab ids across models.
    lm::ngram::GrowableVocab vocab(10, vocab_file.get());
    lm::interpolate::ARPAToStream reader(0, vocab);
    counts = reader.Counts();

    // Configure a chain for each order.  TODO: extract chain balance heuristics from lm/builder/pipeline.cc
    chains.Init(reader.Order());
    for (std::size_t i = 0; i < reader.Order(); ++i) {
      chains.push_back(util::stream::ChainConfig(lm::builder::NGram::TotalSize(i + 1), 2, 1 << 30 /* 1 GB */));
    }
    // Normally >> copies then runs so inline >> works.  But here we want a ref.
    chains >> boost::ref(reader);

    util::stream::SortConfig sort_config;
    sort_config.temp_prefix = "/tmp/";
    sort_config.buffer_size = 1 << 26;
    sort_config.total_memory = 1 << 30;
    // Parallel sorts across orders (though somewhat limited because ARPA files are not being read in parallel across orders)
    lm::builder::Sorts<lm::builder::SuffixOrder> sorts(reader.Order());
    for (std::size_t i = 0; i < reader.Order(); ++i) {
      sorts.push_back(chains[i], sort_config, lm::builder::SuffixOrder(i + 1));
    }
    // Output to the same chains.
    for (std::size_t i = 0; i < reader.Order(); ++i) {
      chains[i].Wait();
      sorts[i].Output(chains[i]);
    }
    // sorts can go out of scope even though it's still writing to the chains.
    // note that vocab going out of scope flushes to vocab_file.
  }

  lm::builder::VocabReconstitute reconstitute(vocab_file.get());
  chains >> lm::builder::PrintARPA(reconstitute, counts, NULL, 1);
  chains.Wait(true);
}
