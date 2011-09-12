#include "lm/left.hh"
#include "lm/model.hh"

#include "util/tokenize_piece.hh"

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
  BOOST_CHECK_EQUAL(2, base.left.valid_length);
  VCheck("more", base.left.words[0]);
  VCheck("loin", base.left.words[1]);
  BOOST_CHECK_EQUAL(1, base.right.valid_length_);
  VCheck("loin", base.right.history_[0]);
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
  BOOST_CHECK_EQUAL(3, more_left.left.valid_length);
  VCheck("little", more_left.left.words[0]);
  VCheck("more", more_left.left.words[1]);
  VCheck("loin", more_left.left.words[2]);
  BOOST_CHECK_EQUAL(1, more_left.right.valid_length_);
  VCheck("loin", more_left.right.history_[0]);
  BOOST_CHECK_EQUAL(false, more_left.charge_backoff);

  ChartState shorter;
  {
    RuleScore<Model> score(m, shorter);
    Term("to");
    score.NonTerminal(base, -1.206319 - 0.3561665);
    BOOST_CHECK_CLOSE(-0.30103 - 1.687872 - 1.206319 - 0.3561665, score.Finish(), 0.01);
  }
  BOOST_CHECK_CLOSE(-1.687872, shorter.left_est, 0.001);
  BOOST_CHECK_EQUAL(1, shorter.left.valid_length);
  VCheck("to", shorter.left.words[0]);
  BOOST_CHECK_EQUAL(1, shorter.right.valid_length_);
  VCheck("loin", shorter.right.history_[0]);
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
  BOOST_CHECK_EQUAL(1, base.left.valid_length);
  VCheck("on", base.left.words[0]);
  BOOST_CHECK_EQUAL(1, base.right.valid_length_);
  VCheck("more", base.right.history_[0]);
  BOOST_CHECK(base.charge_backoff);

  ChartState extend;
  {
    RuleScore<Model> score(m, extend);
    Term("looking");
    score.NonTerminal(base, -1.509559 -0.4771212 -1.206319);
    BOOST_CHECK_CLOSE(-3.91039, score.Finish(), 0.001);
  }
  BOOST_CHECK_EQUAL(2, extend.left.valid_length);
  VCheck("looking", extend.left.words[0]);
  VCheck("on", extend.left.words[1]);
  BOOST_CHECK_EQUAL(1, extend.right.valid_length_);
  VCheck("more", extend.right.history_[0]);
  BOOST_CHECK(extend.charge_backoff);

  ChartState tobos;
  {
    RuleScore<Model> score(m, tobos);
    score.BeginSentence();
    score.NonTerminal(extend, -3.91039);
    BOOST_CHECK_CLOSE(-3.471169, score.Finish(), 0.001);
  }
  BOOST_CHECK_EQUAL(0, tobos.left.valid_length);
  BOOST_CHECK_EQUAL(1, tobos.right.valid_length_);
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
  for (std::vector<WordIndex>::const_reverse_iterator i = words.rbegin(); i != words.rend(); ++i) {
    ChartState copy(state);
    RuleScore<Model> score(m, state);
    score.Terminal(*i);
    score.NonTerminal(copy, ret);
    ret = score.Finish();
  }
  return ret;
}

void LookupVocab(const Model &m, const StringPiece &str, std::vector<WordIndex> &out) {
  out.clear();
  for (util::PieceIterator<' '> i(str); i; ++i) {
    out.push_back(m.GetVocabulary().Index(*i));
  }
}

#define BIDIRECTION(str) \
  LookupVocab(m, str, words); \
  BOOST_CHECK_CLOSE(LeftToRight(m, words), RightToLeft(m, words), 1.5);

// Build sentences, or parts thereof, from right to left.  
BOOST_AUTO_TEST_CASE(GrowLeft) {
  Config config;
  config.messages = NULL;
  Model m("test.arpa", config);

  std::vector<WordIndex> words;
  BIDIRECTION("in biarritz watching considering looking . on a little more loin also would consider higher to look good unknown the screening foo bar , unknown however unknown </s>");
}

} // namespace
} // namespace ngram
} // namespace lm
