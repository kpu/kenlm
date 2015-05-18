#define BOOST_TEST_MODULE InterpolateMergeVocabTest
#include <boost/test/unit_test.hpp>

#include "lm/interpolate/merge_vocab.hh"
#include "lm/lm_exception.hh"
#include "util/file.hh"

#include <cstring>

namespace lm {
namespace interpolate {
namespace {

// Stupid bjam permutes the command line arguments randomly.
class TestFiles {
  public:
    TestFiles() {
      char **argv = boost::unit_test::framework::master_test_suite().argv;
      int argc = boost::unit_test::framework::master_test_suite().argc;
      BOOST_REQUIRE_EQUAL(6, argc);
      for (int i = 1; i < argc; ++i) {
        EndsWithAssign(argv[i], "test1", test[0]);
        EndsWithAssign(argv[i], "test2", test[1]);
        EndsWithAssign(argv[i], "test3", test[2]);
        EndsWithAssign(argv[i], "no_unk", no_unk);
        EndsWithAssign(argv[i], "bad_order", bad_order);
      }
    }

    void EndsWithAssign(char *arg, StringPiece value, util::scoped_fd &to) {
      StringPiece str(arg);
      if (str.size() < value.size()) return;
      if (std::memcmp(str.data() + str.size() - value.size(), value.data(), value.size())) return;
      to.reset(util::OpenReadOrThrow(arg));
    }

    util::scoped_fd test[3], no_unk, bad_order;
};

BOOST_AUTO_TEST_CASE(MergeVocabTest) {
  TestFiles files;
  
  std::vector<lm::interpolate::ModelInfo> vocab_info;
  
  lm::interpolate::ModelInfo m1, m2, m3;
  m1.fd = files.test[0].release();
  m1.vocab_size = 10;
  m2.fd = files.test[1].release();
  m2.vocab_size = 10;
  m3.fd = files.test[2].release();
  m3.vocab_size = 10;
  
  vocab_info.push_back(m1);
  vocab_info.push_back(m2);
  vocab_info.push_back(m3);
  
  std::vector<lm::WordIndex> model_max_idx;
  model_max_idx.push_back(m1.vocab_size);
  model_max_idx.push_back(m2.vocab_size);
  model_max_idx.push_back(m3.vocab_size);
  
  lm::interpolate::UniversalVocab universal_vocab(model_max_idx);
  lm::interpolate::MergeVocabIndex merger(vocab_info, universal_vocab);

  BOOST_CHECK_EQUAL(universal_vocab.GetUniversalIdx(0, 0), 0);
  BOOST_CHECK_EQUAL(universal_vocab.GetUniversalIdx(1, 0), 0);
  BOOST_CHECK_EQUAL(universal_vocab.GetUniversalIdx(2, 0), 0);
  BOOST_CHECK_EQUAL(universal_vocab.GetUniversalIdx(0, 1), 1);
  BOOST_CHECK_EQUAL(universal_vocab.GetUniversalIdx(1, 1), 2);
  BOOST_CHECK_EQUAL(universal_vocab.GetUniversalIdx(2, 1), 8);
  BOOST_CHECK_EQUAL(universal_vocab.GetUniversalIdx(0, 5), 11);
  BOOST_CHECK_EQUAL(universal_vocab.GetUniversalIdx(1, 3), 4);
  BOOST_CHECK_EQUAL(universal_vocab.GetUniversalIdx(2, 3), 10);
}

BOOST_AUTO_TEST_CASE(MergeVocabNoUnkTest) {
  TestFiles files;
  
  std::vector<lm::interpolate::ModelInfo> vocab_info;
  
  lm::interpolate::ModelInfo m1;
  m1.fd = files.no_unk.release();
  m1.vocab_size = 10;
  
  vocab_info.push_back(m1);
  
  std::vector<lm::WordIndex> model_max_idx;
  model_max_idx.push_back(m1.vocab_size);
  
  lm::interpolate::UniversalVocab universal_vocab(model_max_idx);
  BOOST_CHECK_THROW(
    lm::interpolate::MergeVocabIndex merger(vocab_info, universal_vocab),
      lm::FormatLoadException);
    
}

BOOST_AUTO_TEST_CASE(MergeVocabWrongOrderTest) {
  TestFiles files;
  std::vector<lm::interpolate::ModelInfo> vocab_info;

  lm::interpolate::ModelInfo m1, m2;
  m1.fd = files.test[0].release();
  m1.vocab_size = 10;
  m2.fd = files.bad_order.release();
  m2.vocab_size = 10;
  
  vocab_info.push_back(m1);
  vocab_info.push_back(m2);
  
  std::vector<lm::WordIndex> model_max_idx;
  model_max_idx.push_back(m1.vocab_size);
  model_max_idx.push_back(m2.vocab_size);
  
  lm::interpolate::UniversalVocab universal_vocab(model_max_idx);
  BOOST_CHECK_THROW(
    lm::interpolate::MergeVocabIndex merger(vocab_info, universal_vocab),
    util::ErrnoException);
}

}}} // namespaces
