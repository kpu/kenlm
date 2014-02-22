#include "util/stream/chain.hh"
#include "util/stream/io.hh"

int main() {
  util::stream::Chain(util::stream::ChainConfig(1, 2, 64*1024*1024)) >> util::stream::Decompress(0) >> util::stream::WriteAndRecycle(1);
}
