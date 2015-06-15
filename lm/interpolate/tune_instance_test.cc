#include "lm/interpolate/tune_instance.hh"

#include "util/fake_ofstream.hh"
#include "util/file.hh"
#include "util/string_piece.hh"

#define BOOST_TEST_MODULE InstanceTest
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <vector>

namespace lm { namespace interpolate { namespace {

Matrix::Index FindRow(const std::vector<WordIndex> &words, WordIndex word) {
  std::vector<WordIndex>::const_iterator it = std::find(words.begin(), words.end(), word);
  BOOST_REQUIRE(it != words.end());
  return it - words.begin();
}

BOOST_AUTO_TEST_CASE(Toy) {
  util::scoped_fd test_input(util::MakeTemp("temporary"));
  {
    util::FakeOFStream(test_input.get()) << "c\n";
  }

  StringPiece dir("tune_instance_data/");
  if (boost::unit_test::framework::master_test_suite().argc == 2) {
    StringPiece zero_file(boost::unit_test::framework::master_test_suite().argv[1]);
    BOOST_REQUIRE(zero_file.size() > strlen("toy0.1"));
    BOOST_REQUIRE_EQUAL("toy0.1", StringPiece(zero_file.data() + zero_file.size() - 6, 6));
    dir = StringPiece(zero_file.data(), zero_file.size() - 6);
  }

  std::vector<StringPiece> model_names;
  std::string full0 = std::string(dir.data(), dir.size()) + "toy0";
  std::string full1 = std::string(dir.data(), dir.size()) + "toy1";
  model_names.push_back(full0);
  model_names.push_back(full1);

  util::FixedArray<Instance> instances;
  Matrix ln_unigrams;
  // Returns vocab id of <s>
  BOOST_CHECK_EQUAL(1, LoadInstances(test_input.release(), model_names, instances, ln_unigrams));
  // <unk>
  BOOST_CHECK_CLOSE(-0.90309 * M_LN10, ln_unigrams(0, 0), 0.001);
  BOOST_CHECK_CLOSE(-1 * M_LN10, ln_unigrams(0, 1), 0.001);
  // <s>
  BOOST_CHECK_GT(-98.0, ln_unigrams(1, 0));
  BOOST_CHECK_GT(-98.0, ln_unigrams(1, 1));
  // a
  BOOST_CHECK_CLOSE(-0.46943438 * M_LN10, ln_unigrams(2, 0), 0.001);
  BOOST_CHECK_CLOSE(-0.6146491 * M_LN10, ln_unigrams(2, 1), 0.001);
  // </s>
  BOOST_CHECK_CLOSE(-0.5720968 * M_LN10, ln_unigrams(3, 0), 0.001);
  BOOST_CHECK_CLOSE(-0.6146491 * M_LN10, ln_unigrams(3, 1), 0.001);
  // c
  BOOST_CHECK_CLOSE(-0.90309 * M_LN10, ln_unigrams(4, 0), 0.001); // <unk>
  BOOST_CHECK_CLOSE(-0.7659168 * M_LN10, ln_unigrams(4, 1), 0.001);
  // too lazy to do b.

  // Two instances:
  // <s> predicts c
  // <s> c predicts </s>
  BOOST_REQUIRE_EQUAL(2, instances.size());
  BOOST_CHECK_CLOSE(-0.30103 * M_LN10, instances[0].ln_backoff(0), 0.001);
  BOOST_CHECK_CLOSE(-0.30103 * M_LN10, instances[0].ln_backoff(1), 0.001);

  // Backoffs of <s> c
  BOOST_CHECK_CLOSE(0.0, instances[1].ln_backoff(0), 0.001);
  BOOST_CHECK_CLOSE((-0.30103 - 0.30103) * M_LN10, instances[1].ln_backoff(1), 0.001);

  // Three extensions: a, b, c
  BOOST_REQUIRE_EQUAL(3, instances[0].ln_extensions.rows());
  BOOST_REQUIRE_EQUAL(3, instances[0].extension_words.size());

  // <s> a
  BOOST_CHECK_CLOSE(-0.37712017 * M_LN10, instances[0].ln_extensions(FindRow(instances[0].extension_words, 2), 0), 0.001);
  // <s> c
  BOOST_CHECK_CLOSE((-0.90309 + -0.30103) * M_LN10, instances[0].ln_extensions(FindRow(instances[0].extension_words, 4), 0), 0.001);
  BOOST_CHECK_CLOSE(-0.4740302 * M_LN10, instances[0].ln_extensions(FindRow(instances[0].extension_words, 4), 1), 0.001);

  // <s> c </s>
  BOOST_CHECK_CLOSE(-0.09113217 * M_LN10, instances[1].ln_extensions(FindRow(instances[1].extension_words, 3), 1), 0.001);

  // p_0(c | <s>) = p_0(c)b_0(<s>) = 10^(-0.90309 + -0.30103)
  BOOST_CHECK_CLOSE((-0.90309 + -0.30103) * M_LN10, instances[0].ln_correct(0), 0.001);
  // p_1(c | <s>) = 10^-0.4740302
  BOOST_CHECK_CLOSE(-0.4740302 * M_LN10, instances[0].ln_correct(1), 0.001);
}

}}} // namespaces
