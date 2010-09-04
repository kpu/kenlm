#include "lm/ngram.hh"

#define BOOST_TEST_MODULE NGramTest

#include <boost/test/unit_test.hpp>

namespace lm {
namespace ngram {
namespace {

struct Fixture {
	Fixture() : model("test.arpa") {}

	Model model;

	unsigned int Lookup(const char *value) const {
		return model.GetVocabulary().Index(StringPiece(value));
	}
};

BOOST_FIXTURE_TEST_SUITE(s, Fixture)

BOOST_AUTO_TEST_CASE(starters) {
	Model::State state(model.BeginSentenceState());
	Model::State out;
	std::vector<WordIndex> only_begin;
  only_begin.push_back(model.GetVocabulary().BeginSentence());

	BOOST_CHECK_CLOSE(-0.4846522,
			model.IncrementalScore(
				state,
				only_begin.rbegin(),
				Lookup("looking"),
				out) / M_LN10, 0.00001);
	BOOST_CHECK_EQUAL((unsigned int)2, out.NGramLength());
			
	// , probability plus <s> backoff
	BOOST_CHECK_CLOSE(-1.383514 + -0.4149733,
			model.IncrementalScore(
				state,
				only_begin.rbegin(),
				Lookup(","),
				out) / M_LN10, 0.0001);
	BOOST_CHECK_EQUAL((unsigned int)1, out.NGramLength());

	BOOST_CHECK_CLOSE(-1.995635 + -0.4149733,
			model.IncrementalScore(
				state,
				only_begin.rbegin(),
				Lookup("this_is_not_found"),
				out) / M_LN10, 0.00001);
	BOOST_CHECK_EQUAL((unsigned int)0, out.NGramLength());
}

#define AppendTest(word, ngram, score) \
	index = Lookup(word); \
	BOOST_CHECK_CLOSE((score), \
			model.IncrementalScore( \
				state, \
				history.rbegin(), \
				index, \
				out) / M_LN10, 0.001); \
	BOOST_CHECK_EQUAL(static_cast<unsigned int>(ngram), out.NGramLength()); \
	history.push_back(index); \
	state = out;

BOOST_AUTO_TEST_CASE(continuation) {
	Model::State state(model.BeginSentenceState());
	Model::State out;

	std::vector<WordIndex> history;
  history.push_back(model.GetVocabulary().BeginSentence());

	WordIndex index;

	AppendTest("looking", 2,-0.484652);
	AppendTest("on", 3,-0.348837);
	AppendTest("a", 4,-0.0155266);
	AppendTest("little", 5,-0.00306122);
	AppendTest("the", 1,-4.04005);
	AppendTest("biarritz", 1,-1.9889);
	AppendTest("not_found", 0,-2.29666);
	AppendTest("more", 1,-1.20632);
	AppendTest(".", 2,-0.51363);
	AppendTest("</s>", 3,-0.0191651);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
} // namespace ngram
} // namespace lm
