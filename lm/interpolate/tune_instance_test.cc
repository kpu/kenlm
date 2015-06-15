#include "lm/interpolate/tune_instance.hh"

#include "util/fake_ofstream.hh"
#include "util/file.hh"
#include "util/string_piece.hh"

#define BOOST_TEST_MODULE InstanceTest
#include <boost/test/unit_test.hpp>

#include <iostream>

#include <vector>

namespace lm { namespace interpolate { namespace {

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
  LoadInstances(test_input.release(), model_names, instances, ln_unigrams);
}

}}} // namespaces
