#define BOOST_TEST_MODULE InterpolateMergeVocabTest
#include <boost/test/unit_test.hpp>

#include "lm/interpolate/merge_vocab.hh"
#include "lm/lm_exception.hh"


namespace lm {
namespace interpolate {

namespace {

  class my_excaption{};

// Stupid bjam reverses the command line arguments randomly.
// const char *TestLocation() {
//   if (boost::unit_test::framework::master_test_suite().argc < 3) {
//     return "test.arpa";
//   }
//   char **argv = boost::unit_test::framework::master_test_suite().argv;
//   return argv[strstr(argv[1], "nounk") ? 2 : 1];
// }
// const char *TestNoUnkLocation() {
//   if (boost::unit_test::framework::master_test_suite().argc < 3) {
//     return "test_nounk.arpa";
//   }
//   char **argv = boost::unit_test::framework::master_test_suite().argv;
//   return argv[strstr(argv[1], "nounk") ? 1 : 2];
// }

BOOST_AUTO_TEST_CASE(MergeVocabTest) {
  
  // const char * file_name_1 = TestLocation();
  // const char * file_name_2 = TestNoUnkLocation();

  // if ((fp1 = fopen(file_name_1, "r")) == NULL)
  //   return -1;
  // if ((fp2 = fopen(file_name_2, "r")) == NULL)
  //   return -1;
  // if ((fp3 = fopen("test3", "r")) == NULL)
  //   return -1;


  std::vector<lm::interpolate::ModelInfo> vocab_info;
  FILE *fp1, *fp2, *fp3;

  if ((fp1 = fopen("test1", "r")) == NULL)
    throw;
  if ((fp2 = fopen("test2", "r")) == NULL)
    throw;
  if ((fp3 = fopen("test3", "r")) == NULL)
    throw;
  
  lm::interpolate::ModelInfo m1, m2, m3;
  m1.fd = fileno(fp1);
  m1.vocab_size = 10;
  m2.fd = fileno(fp2);
  m2.vocab_size = 10;
  m3.fd = fileno(fp3);
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
  
  std::vector<lm::interpolate::ModelInfo> vocab_info;
  FILE *fp1;

  if ((fp1 = fopen("test_no_unk", "r")) == NULL)
    throw;
  
  lm::interpolate::ModelInfo m1;
  m1.fd = fileno(fp1);
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
  std::vector<lm::interpolate::ModelInfo> vocab_info;
  FILE *fp1, *fp2;

  if ((fp1 = fopen("test1", "r")) == NULL)
    throw;
  if ((fp2 = fopen("test_bad_order", "r")) == NULL)
    throw;

  lm::interpolate::ModelInfo m1, m2;
  m1.fd = fileno(fp1);
  m1.vocab_size = 10;
  m2.fd = fileno(fp2);
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

}}} // namespace
