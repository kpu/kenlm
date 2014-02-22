#include "lm/builder/sort.hh"
#include "util/stream/config.hh"
#include "util/stream/sort.hh"

using namespace lm::builder;

int main(int argc, char *argv[]) {
  util::stream::FileMergingReader<SuffixOrder, AddCombiner> reader(SuffixOrder(5), AddCombiner());
  for (unsigned int i = 1; i < argc, ++i) {
    reader.Add(argv[i]);
  }
  util::stream::Chain(util::stream::ChainConfig(5*4 + 8, 2, 1048576)) >> boost::ref(reader) >> util::stream::Write(1);
}
