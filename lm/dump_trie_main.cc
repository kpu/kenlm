#include "lm/config.hh"
#include "lm/model.hh"
#include "util/fake_ofstream.hh"
#include "util/file.hh"
#include "util/ersatz_progress.hh"
#include "util/multi_intersection.hh"
#include "util/tokenize_piece.hh"

#include <boost/lexical_cast.hpp>

#include <iostream>
#include <string.h>

namespace lm {

struct NoOpProgress {
  NoOpProgress(uint64_t end) {}
  void operator++() const {}
};

struct Seen {
  StringPiece value;
  std::vector<unsigned> sentences;
};

class DumpTrie {
  public:
    DumpTrie() {}

    void Dump(ngram::TrieModel &model, const std::string &base, const std::string &vocab) {
      seen_.resize(model.GetVocabulary().Bound());
      util::FilePiece f(vocab.c_str());
      unsigned l;
      try { for (l = 0; ; ++l) {
        StringPiece line = f.ReadLine();
        for (util::TokenIter<util::BoolCharacter, true> word(line, util::kSpaces); word; ++word) {
          Seen &stored = seen_[model.GetVocabulary().Index(*word)];
          if (stored.value.empty()) {
            stored.value = StringPiece(static_cast<const char*>(memcpy(pool_.Allocate(word->size()), word->data(), word->size())), word->size());
          }
          stored.sentences.push_back(l);
        }
      } } catch (const util::EndOfFileException &e) {}
      // <s> and </s> always appear
      Seen &bos = seen_[model.GetVocabulary().BeginSentence()], &eos = seen_[model.GetVocabulary().EndSentence()];
      bos.value = "<s>";
      eos.value = "</s>";
      bos.sentences.resize(l);
      for (unsigned i = 0; i < l; ++i) bos.sentences[i] = i;
      eos.sentences = bos.sentences;

      ngram::trie::NodeRange range;
      range.begin = 0;
      range.end = model.GetVocabulary().Bound();
      for (unsigned char i = 0; i < model.Order(); ++i) {
        count_[i] = 0;
        files_[i].reset(util::CreateOrThrow((base + boost::lexical_cast<std::string>(static_cast<unsigned int>(i) + 1)).c_str()));
        out_[i].SetFD(files_[i].get());
      }
      Dump<util::ErsatzProgress>(model, 1, range);
      for (unsigned char i = 0; i < model.Order(); ++i) {
        std::cout << "ngram " << static_cast<unsigned>(i) << '=' << count_[i] << '\n';
      }
    }

  private:
    template <class Progress> void Dump(ngram::TrieModel &model, const unsigned char order, const ngram::trie::NodeRange range) {
      ngram::trie::NodeRange pointer;
      ProbBackoff weights;
      WordIndex word;
      Progress progress(range.end);
      for (uint64_t i = range.begin; i < range.end; ++i, ++progress) {
        model.search_.CheckedRead(order, i, word, weights, pointer);
        if (seen_[word].value.empty()) continue;
        sets_.push_back(boost::iterator_range<std::vector<unsigned>::const_iterator>(seen_[word].sentences.begin(), seen_[word].sentences.end()));
        sets_copy_ = sets_;
        if (util::FirstIntersection(sets_copy_)) {
          words_[order - 1] = seen_[word].value;
          ++count_[order - 1];
          out_[order - 1] << weights.prob << '\t' << words_[order - 1];
          for (char w = static_cast<char>(order) - 2; w >= 0; --w) {
            out_[static_cast<unsigned char>(order - 1)] << ' ' << words_[static_cast<unsigned char>(w)];
          }
          out_[order - 1] << '\t' << weights.backoff << '\n';
          Dump<NoOpProgress>(model, order + 1, pointer);
        }
        sets_.pop_back();
      }
    }

    util::Pool pool_;
    std::vector<Seen> seen_;
    util::scoped_fd files_[KENLM_MAX_ORDER];
    util::FakeOFStream out_[KENLM_MAX_ORDER];

    StringPiece words_[KENLM_MAX_ORDER];

    uint64_t count_[KENLM_MAX_ORDER];

    std::vector<boost::iterator_range<std::vector<unsigned>::const_iterator> > sets_, sets_copy_;
};
} // namespace

int main(int argc, char *argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0] << " trie output_base vocab" << std::endl;
    return 1;
  }

  lm::ngram::Config config;
  config.load_method = util::LAZY;
  lm::ngram::TrieModel model(argv[1], config);
  lm::DumpTrie dumper;
  dumper.Dump(model, argv[2], argv[3]);
  return 0;
}
