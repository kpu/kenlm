#include "lm/ngram.hh"

#define BOOST_TEST_MODULE NGramTest

#include <boost/test/unit_test.hpp>

namespace lm {
namespace ngram {
namespace {

struct Fixture {
	Fixture() : owner("test.arpa"), model(owner.GetModel()) {}

  Owner owner;
  const Model &model;

	unsigned int Lookup(const char *value) const {
		return model.GetVocabulary().Index(StringPiece(value));
	}
};

BOOST_FIXTURE_TEST_SUITE(s, Fixture)

#define StartTest(word, ngram, score) \
  ret = model.WithLength( \
			state, \
			Lookup(word), \
			out);\
  BOOST_CHECK_CLOSE(score, ret.prob / M_LN10, 0.001); \
	BOOST_CHECK_EQUAL(static_cast<unsigned int>(ngram), ret.ngram_length); \
  BOOST_CHECK_EQUAL(std::min<unsigned char>(ngram, 5 - 1), out.valid_length_);

BOOST_AUTO_TEST_CASE(starters) {
	Model::State state(model.BeginSentenceState());
	Model::State out;
  Return ret;

  StartTest("looking", 2, -0.4846522);

	// , probability plus <s> backoff
  StartTest(",", 1, -1.383514 + -0.4149733);
	// <unk> probability plus <s> backoff
  StartTest("this_is_not_found", 0, -1.995635 + -0.4149733);
}

#define AppendTest(word, ngram, score) \
  StartTest(word, ngram, score) \
	state = out;

BOOST_AUTO_TEST_CASE(continuation) {
  Return ret;
	Model::State state(model.BeginSentenceState());
	Model::State out;

	AppendTest("looking", 2, -0.484652);
	AppendTest("on", 3, -0.348837);
	AppendTest("a", 4, -0.0155266);
	AppendTest("little", 5, -0.00306122);
  State preserve = state;
	AppendTest("the", 1, -4.04005);
	AppendTest("biarritz", 1, -1.9889);
	AppendTest("not_found", 0, -2.29666);
	AppendTest("more", 1, -1.20632);
	AppendTest(".", 2, -0.51363);
	AppendTest("</s>", 3, -0.0191651);

  state = preserve;
  AppendTest("more", 5, -0.00181395);
  AppendTest("loin", 5, -0.0432557);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
} // namespace ngram
} // namespace lm
