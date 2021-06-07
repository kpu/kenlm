#include "count_records.hh"
#include "chain.hh"

namespace util { namespace stream {

void CountRecords::Run(const ChainPosition &position) {
  for (Link link(position); link; ++link) {
    *count_ += link->ValidSize() / position.GetChain().EntrySize();
  }
}

}} // namespaces
