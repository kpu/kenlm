#include "lm/model.hh"
#include "lm/left.hh"
#include "util/tokenize_piece.hh"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Expected model file name." << std::endl;
    return 1;
  }
  lm::ngram::RestProbingModel model(argv[1]);
  std::string line;
  lm::ngram::ChartState ignored;
  while (getline(std::cin, line)) {
    lm::ngram::RuleScore<lm::ngram::RestProbingModel> scorer(model, ignored);
    for (util::TokenIter<util::SingleCharacter, true> i(line, ' '); i; ++i) {
      scorer.Terminal(model.GetVocabulary().Index(*i));
    }
    std::cout << scorer.Finish() << '\n';
  }
}
