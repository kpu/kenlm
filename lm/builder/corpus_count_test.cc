#include "lm/builder/corpus_count.hh"

#include "lm/builder/ngram.hh"

#include "util/file.hh"
#include "util/file_piece.hh"
#include "util/stream/chain.hh"
#include "util/stream/stream.hh"

#define BOOST_TEST_MODULE CorpusCountTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

namespace lm { namespace builder { namespace {

BOOST_AUTO_TEST_CASE(Short) {
  util::TempMaker temp("corpus_count_test_temp");
  util::scoped_fd input_file(temp.Make());
  const char input[] = "looking on a little more loin\non a little more loin test\nlooking on a little more\nfoo\n";
  util::WriteOrThrow(input_file.get(), input, sizeof(input));
  util::FilePiece input_piece(input_file.release(), "temp file");

  char vocab_name[] = "corpus_count_vocabXXXXXX";
  util::scoped_fd vocab(mkstemp(vocab_name));

  util::stream::ChainConfig config;
  config.entry_size = NGram::TotalSize(3);
  config.block_size = config.entry_size * 9;
  config.block_count = 4;
  config.queue_length = 2;

  util::stream::Chain chain(config);
  util::stream::Stream stream;
  chain >> CorpusCount(input_piece, 3, vocab_name) >> stream >> util::stream::kRecycle;
  BOOST_REQUIRE(stream);

}

}}} // namespaces
