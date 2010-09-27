#include "lm/ngram.hh"

#include <stdlib.h>

#define BOOST_TEST_MODULE NGramTest
#include <boost/test/unit_test.hpp>

namespace lm {
namespace ngram {
namespace {

#define StartTest(word, ngram, score) \
  ret = model.FullScore( \
      state, \
      model.GetVocabulary().Index(word), \
      out);\
  BOOST_CHECK_CLOSE(score, ret.prob, 0.001); \
  BOOST_CHECK_EQUAL(static_cast<unsigned int>(ngram), ret.ngram_length); \
  BOOST_CHECK_EQUAL(std::min<unsigned char>(ngram, 5 - 1), out.valid_length_);

#define AppendTest(word, ngram, score) \
  StartTest(word, ngram, score) \
  state = out;

template <class M> void Starters(const M &model) {
  FullScoreReturn ret;
  Model::State state(model.BeginSentenceState());
  Model::State out;

  StartTest("looking", 2, -0.4846522);

  // , probability plus <s> backoff
  StartTest(",", 1, -1.383514 + -0.4149733);
  // <unk> probability plus <s> backoff
  StartTest("this_is_not_found", 0, -1.995635 + -0.4149733);
}

template <class M> void Continuation(const M &model) {
  FullScoreReturn ret;
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

#define StatelessTest(begin, end, ngram, score) \
  ret = model.SlowStatelessScore(begin, end); \
  BOOST_CHECK_CLOSE(score, ret.prob, 0.001); \
  BOOST_CHECK_EQUAL(static_cast<unsigned int>(ngram), ret.ngram_length);

template <class M> void Stateless(const M &model) {
  const char *words[] = {"<s>", "looking", "on", "a", "little", "the", "biarritz", "not_found", "more", ".", "</s>"};
  WordIndex indices[sizeof(words) / sizeof(const char*)];
  for (unsigned int i = 0; i < sizeof(words) / sizeof(const char*); ++i) {
    indices[i] = model.GetVocabulary().Index(words[i]);
  }
  FullScoreReturn ret;
  StatelessTest(indices, indices + 2, 2, -0.484652);
  StatelessTest(indices, indices + 3, 3, -0.348837);
  StatelessTest(indices, indices + 4, 4, -0.0155266);
  StatelessTest(indices, indices + 5, 5, -0.00306122);
  // the
  StatelessTest(indices, indices + 6, 1, -4.04005);
  StatelessTest(indices + 1, indices + 6, 1, -4.04005);
  // biarritz
  StatelessTest(indices, indices + 7, 1, -1.9889);
  // not found
  StatelessTest(indices, indices + 8, 0, -2.29666);
}

BOOST_AUTO_TEST_CASE(probing) {
  Model m("test.arpa");
  Starters(m);
  Continuation(m);
  Stateless(m);
}
BOOST_AUTO_TEST_CASE(sorted) {
  SortedModel m("test.arpa");
  Starters(m);
  Continuation(m);
  Stateless(m);
}

BOOST_AUTO_TEST_CASE(write_and_read_probing) {
  Config config;
  config.write_mmap = "test.binary";
  {
    Model copy_model("test.arpa", config);
  }
  Model binary("test.binary");
  Starters(binary);
  Continuation(binary);
  Stateless(binary);
}

BOOST_AUTO_TEST_CASE(write_and_read_sorted) {
  Config config;
  config.write_mmap = "test.binary";
  config.prefault = true;
  {
    SortedModel copy_model("test.arpa", config);
  }
  SortedModel binary("test.binary");
  Starters(binary);
  Continuation(binary);
  Stateless(binary);
}


} // namespace
} // namespace ngram
} // namespace lm
