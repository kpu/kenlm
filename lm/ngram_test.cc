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

#define StatelessTest(word, provide, ngram, score) \
  ret = model.FullScoreForgotState(indices + num_words - word, indices + num_words - word + provide, indices[num_words - word - 1], state); \
  BOOST_CHECK_CLOSE(score, ret.prob, 0.001); \
  BOOST_CHECK_EQUAL(static_cast<unsigned int>(ngram), ret.ngram_length); \
  model.GetState(indices + num_words - word, indices + num_words - word + provide, before); \
  ret = model.FullScore(before, indices[num_words - word - 1], out); \
  BOOST_CHECK(state == out); \
  BOOST_CHECK_CLOSE(score, ret.prob, 0.001); \
  BOOST_CHECK_EQUAL(static_cast<unsigned int>(ngram), ret.ngram_length);

template <class M> void Stateless(const M &model) {
  const char *words[] = {"<s>", "looking", "on", "a", "little", "the", "biarritz", "not_found", "more", ".", "</s>"};
  const size_t num_words = sizeof(words) / sizeof(const char*);
  // Silience "array subscript is above array bounds" when extracting end pointer.
  WordIndex indices[num_words + 1];
  for (unsigned int i = 0; i < num_words; ++i) {
    indices[num_words - 1 - i] = model.GetVocabulary().Index(words[i]);
  }
  FullScoreReturn ret;
  State state, out, before;

  ret = model.FullScoreForgotState(indices + num_words - 1, indices + num_words, indices[num_words - 2], state);
  BOOST_CHECK_CLOSE(-0.484652, ret.prob, 0.001);
  StatelessTest(1, 1, 2, -0.484652);

  // looking
  StatelessTest(1, 2, 2, -0.484652);
  // on
  AppendTest("on", 3, -0.348837);
  StatelessTest(2, 3, 3, -0.348837);
  StatelessTest(2, 2, 3, -0.348837);
  StatelessTest(2, 1, 2, -0.4638903);
  // a
  StatelessTest(3, 4, 4, -0.0155266);
  // little
  AppendTest("little", 5, -0.00306122);
  StatelessTest(4, 5, 5, -0.00306122);
  // the
  AppendTest("the", 1, -4.04005);
  StatelessTest(5, 5, 1, -4.04005);
  // No context of the.  
  StatelessTest(5, 0, 1, -1.687872);
  // biarritz
  StatelessTest(6, 1, 1, -1.9889);
  // not found
  StatelessTest(7, 1, 0, -2.29666);
  StatelessTest(7, 0, 0, -1.995635);

  WordIndex unk[1];
  unk[0] = 0;
  model.GetState(unk, unk + 1, state);
  BOOST_CHECK_EQUAL(0, state.valid_length_);
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
