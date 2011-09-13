#include "lm/left.hh"
#include "lm/model.hh"

#include "util/tokenize_piece.hh"

#include <vector>

#define BOOST_TEST_MODULE LeftTest
#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>

namespace lm {
namespace ngram {
namespace {

#define Term(word) score.Terminal(m.GetVocabulary().Index(word));
#define VCheck(word, value) BOOST_CHECK_EQUAL(m.GetVocabulary().Index(word), value);

BOOST_AUTO_TEST_CASE(Short) {
  Config config;
  config.messages = NULL;
  Model m("test.arpa", config);

  ChartState base;
  {
    RuleScore<Model> score(m, base);
    Term("more");
    Term("loin");
    BOOST_CHECK_CLOSE(-1.206319 - 0.3561665, score.Finish(), 0.001);
  }
  BOOST_CHECK_EQUAL(false, base.charge_backoff);
  BOOST_CHECK_CLOSE(-1.206319 - 0.3561665, base.left_est, 0.001);
  BOOST_CHECK_EQUAL(2, base.left.length);
  VCheck("more", base.left.words[0]);
  VCheck("loin", base.left.words[1]);
  BOOST_CHECK_EQUAL(1, base.right.length);
  VCheck("loin", base.right.words[0]);
  BOOST_CHECK_EQUAL(false, base.charge_backoff);

  ChartState more_left;
  {
    RuleScore<Model> score(m, more_left);
    Term("little");
    score.NonTerminal(base, -1.206319 - 0.3561665);
    // p(little more loin | null context)
    BOOST_CHECK_CLOSE(-1.56538, score.Finish(), 0.001);
  }
  BOOST_CHECK_CLOSE(-1.56538, more_left.left_est, 0.001);
  BOOST_CHECK_EQUAL(3, more_left.left.length);
  VCheck("little", more_left.left.words[0]);
  VCheck("more", more_left.left.words[1]);
  VCheck("loin", more_left.left.words[2]);
  BOOST_CHECK_EQUAL(1, more_left.right.length);
  VCheck("loin", more_left.right.words[0]);
  BOOST_CHECK_EQUAL(false, more_left.charge_backoff);

  ChartState shorter;
  {
    RuleScore<Model> score(m, shorter);
    Term("to");
    score.NonTerminal(base, -1.206319 - 0.3561665);
    BOOST_CHECK_CLOSE(-0.30103 - 1.687872 - 1.206319 - 0.3561665, score.Finish(), 0.01);
  }
  BOOST_CHECK_CLOSE(-1.687872, shorter.left_est, 0.001);
  BOOST_CHECK_EQUAL(1, shorter.left.length);
  VCheck("to", shorter.left.words[0]);
  BOOST_CHECK_EQUAL(1, shorter.right.length);
  VCheck("loin", shorter.right.words[0]);
  BOOST_CHECK(shorter.charge_backoff);
}

BOOST_AUTO_TEST_CASE(Charge) {
  Config config;
  config.messages = NULL;
  Model m("test.arpa", config);

  ChartState base;
  {
    RuleScore<Model> score(m, base);
    Term("on");
    Term("more");
    BOOST_CHECK_CLOSE(-1.509559 -0.4771212 -1.206319, score.Finish(), 0.001);
  }
  BOOST_CHECK_EQUAL(1, base.left.length);
  VCheck("on", base.left.words[0]);
  BOOST_CHECK_EQUAL(1, base.right.length);
  VCheck("more", base.right.words[0]);
  BOOST_CHECK(base.charge_backoff);

  ChartState extend;
  {
    RuleScore<Model> score(m, extend);
    Term("looking");
    score.NonTerminal(base, -1.509559 -0.4771212 -1.206319);
    BOOST_CHECK_CLOSE(-3.91039, score.Finish(), 0.001);
  }
  BOOST_CHECK_EQUAL(2, extend.left.length);
  VCheck("looking", extend.left.words[0]);
  VCheck("on", extend.left.words[1]);
  BOOST_CHECK_EQUAL(1, extend.right.length);
  VCheck("more", extend.right.words[0]);
  BOOST_CHECK(extend.charge_backoff);

  ChartState tobos;
  {
    RuleScore<Model> score(m, tobos);
    score.BeginSentence();
    score.NonTerminal(extend, -3.91039);
    BOOST_CHECK_CLOSE(-3.471169, score.Finish(), 0.001);
  }
  BOOST_CHECK_EQUAL(0, tobos.left.length);
  BOOST_CHECK_EQUAL(1, tobos.right.length);
}

float LeftToRight(const Model &m, const std::vector<WordIndex> &words) {
  float ret = 0.0;
  State right = m.NullContextState();
  for (std::vector<WordIndex>::const_iterator i = words.begin(); i != words.end(); ++i) {
    State copy(right);
    ret += m.Score(copy, *i, right);
  }
  return ret;
}

float RightToLeft(const Model &m, const std::vector<WordIndex> &words) {
  float ret = 0.0;
  ChartState state;
  state.left.length = 0;
  state.right.length = 0;
  state.charge_backoff = false;
  state.left_est = 0.0;
  for (std::vector<WordIndex>::const_reverse_iterator i = words.rbegin(); i != words.rend(); ++i) {
    ChartState copy(state);
    RuleScore<Model> score(m, state);
    score.Terminal(*i);
    score.NonTerminal(copy, ret);
    ret = score.Finish();
  }
  return ret;
}

float TreeMiddle(const Model &m, const std::vector<WordIndex> &words) {
  std::vector<std::pair<ChartState, float> > states(words.size());
  for (unsigned int i = 0; i < words.size(); ++i) {
    RuleScore<Model> score(m, states[i].first);
    score.Terminal(words[i]);
    states[i].second = score.Finish();
  }
  while (states.size() > 1) {
    std::vector<std::pair<ChartState, float> > upper((states.size() + 1) / 2);
    for (unsigned int i = 0; i < states.size() / 2; ++i) {
      RuleScore<Model> score(m, upper[i].first);
      score.NonTerminal(states[i*2].first, states[i*2].second);
      score.NonTerminal(states[i*2+1].first, states[i*2+1].second);
      upper[i].second = score.Finish();
    }
    if (states.size() % 2) {
      upper.back() = states.back();
    }
    std::swap(states, upper);
  }
  return states.empty() ? 0 : states.back().second;
}

void LookupVocab(const Model &m, const StringPiece &str, std::vector<WordIndex> &out) {
  out.clear();
  for (util::PieceIterator<' '> i(str); i; ++i) {
    out.push_back(m.GetVocabulary().Index(*i));
  }
}

#define TEXT_TEST(str) \
{ \
  std::vector<WordIndex> words; \
  LookupVocab(m, str, words); \
  float expect = LeftToRight(m, words); \
  BOOST_CHECK_CLOSE(expect, RightToLeft(m, words), 0.001); \
  BOOST_CHECK_CLOSE(expect, TreeMiddle(m, words), 0.001); \
}

// Build sentences, or parts thereof, from right to left.  
BOOST_AUTO_TEST_CASE(GrowBig) {
  Config config;
  config.messages = NULL;
  Model m("test.arpa", config);

  TEXT_TEST("in biarritz watching considering looking . on a little more loin also would consider higher to look good unknown the screening foo bar , unknown however unknown </s>");
}

BOOST_AUTO_TEST_CASE(GrowSmall) {
  Config config;
  config.messages = NULL;
  Model m("test.arpa", config);

  TEXT_TEST("in biarritz watching considering looking . </s>");
  TEXT_TEST("in biarritz watching considering looking .");
  TEXT_TEST("in biarritz");
}

#define CHECK_SCORE(str, val) \
{ \
  float got = val; \
  std::vector<WordIndex> indices; \
  LookupVocab(m, str, indices); \
  BOOST_CHECK_CLOSE(LeftToRight(m, indices), got, 0.001); \
}

BOOST_AUTO_TEST_CASE(FullGrow) {
  Config config;
  config.messages = NULL;
  Model m("test.arpa", config);

  std::vector<WordIndex> words;
  LookupVocab(m, "in biarritz watching considering looking . </s>", words);

  ChartState lexical[7];
  float lexical_scores[7];
  for (unsigned int i = 0; i < 7; ++i) {
    RuleScore<Model> score(m, lexical[i]);
    score.Terminal(words[i]);
    lexical_scores[i] = score.Finish();
  }
  CHECK_SCORE("in", lexical_scores[0]);
  CHECK_SCORE("biarritz", lexical_scores[1]);
  CHECK_SCORE("watching", lexical_scores[2]);
  CHECK_SCORE("</s>", lexical_scores[6]);

  ChartState l1[4];
  float l1_scores[4];
  {
    RuleScore<Model> score(m, l1[0]);
    score.NonTerminal(lexical[0], lexical_scores[0]);
    score.NonTerminal(lexical[1], lexical_scores[1]);
    CHECK_SCORE("in biarritz", l1_scores[0] = score.Finish());
  }
  {
    RuleScore<Model> score(m, l1[1]);
    score.NonTerminal(lexical[2], lexical_scores[2]);
    score.NonTerminal(lexical[3], lexical_scores[3]);
    CHECK_SCORE("watching considering", l1_scores[1] = score.Finish());
  }
  {
    RuleScore<Model> score(m, l1[2]);
    score.NonTerminal(lexical[4], lexical_scores[4]);
    score.NonTerminal(lexical[5], lexical_scores[5]);
    CHECK_SCORE("looking .", l1_scores[2] = score.Finish());
  }
  BOOST_CHECK_EQUAL(l1[2].left.length, 1);
  l1[3] = lexical[6];
  l1_scores[3] = lexical_scores[6];

  ChartState l2[2];
  float l2_scores[2];
  {
    RuleScore<Model> score(m, l2[0]);
    score.NonTerminal(l1[0], l1_scores[0]);
    score.NonTerminal(l1[1], l1_scores[1]);
    CHECK_SCORE("in biarritz watching considering", l2_scores[0] = score.Finish());
  }
  {
    RuleScore<Model> score(m, l2[1]);
    score.NonTerminal(l1[2], l1_scores[2]);
    score.NonTerminal(l1[3], l1_scores[3]);
    CHECK_SCORE("looking . </s>", l2_scores[1] = score.Finish());
  }
  BOOST_CHECK_EQUAL(l2[1].left.length, 1);
  VCheck("looking", l2[1].left.words[0]);
  BOOST_CHECK(l2[1].charge_backoff);

  ChartState top;
  {
    RuleScore<Model> score(m, top);
    score.NonTerminal(l2[0], l2_scores[0]);
    score.NonTerminal(l2[1], l2_scores[1]);
    CHECK_SCORE("in biarritz watching considering looking . </s>", score.Finish());
  }
}

} // namespace
} // namespace ngram
} // namespace lm
