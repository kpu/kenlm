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
  std::vector<unsigned> sentences;
  WordIndex new_id;
};


class VocabRemember {
  public:
    VocabRemember() {}

    void AddUnique(StringPiece word, WordIndex old_index) {
      char *dest = static_cast<char*>(strings_.Allocate(word.size() + 1));
      memcpy(dest, word.data(), word.size());
      dest[word.size()] = '\0';
      entries_.resize(entries_.size() + 1);
      entries_.back().word = dest;
      entries_.back().old_id = old_index;
    }

    std::size_t Size() {
      return entries_.size();
    }

    uint64_t *Finish(uint64_t *new_vocab, std::vector<Seen> &set_new) {
      std::sort(entries_.begin(), entries_.end());
      util::scoped_fd f(util::CreateOrThrow("filtered_vocab"));
      util::WriteOrThrow(f.get(), "<unk>", 6);
      for (std::vector<Entry>::const_iterator i = entries_.begin() + 1; i != entries_.end(); ++i, ++new_vocab) {
        util::WriteOrThrow(f.get(), i->word, strlen(i->word) + 1);
        *new_vocab = util::MurmurHash64A(i->word, strlen(i->word));
        set_new[i->old_id].new_id = i - entries_.begin();
      }
      strings_.FreeAll();
      return new_vocab;
    }

  private:
    struct Entry {
      WordIndex old_id;
      const char *word;

      bool operator<(const Entry &other) const {
        return old_id < other.old_id;
      }
    };

    util::Pool strings_;

    std::vector<Entry> entries_;
};

typedef lm::ngram::ArrayTrieModel Building;

class JustCount {
  public:
    explicit JustCount(uint64_t *out) : counts_(out - 1) {}

    void operator()(const WordIndex *begin, const WordIndex *end, const ProbBackoff &) const {
      ++counts_[end - begin];
    }

  private:
    uint64_t *counts_;
};

class WriteTrie {
  public:
    explicit WriteTrie(Building &building) : building_(building) {}

    void operator()(const WordIndex *begin, const WordIndex *end, const ProbBackoff &weights) const {
      building_.search_.ExternalInsert(end - begin, *begin, weights);
    }

  private:
    Building &building_;
};

class DumpTrie {
  public:
    DumpTrie() {}

    void Dump(ngram::TrieModel &model, const std::string &base, const std::string &vocab) {
      seen_.resize(model.GetVocabulary().Bound());
      util::FilePiece f(vocab.c_str());
      unsigned l;
      StringPiece word;
      VocabRemember vocab_remember;

      // <unk> appears somewhere.
      vocab_remember.AddUnique("<unk>", 0);
      seen_[0].sentences.push_back(0);
      seen_[0].new_id = 0;

      try { for (l = 1 /* <unk> is first sentence */; ; ++l, f.ReadLine()) {
        while (f.ReadWordSameLine(word)) {
          WordIndex index = model.GetVocabulary().Index(word);
          Seen &stored = seen_[index];
          if (stored.sentences.empty()) {
            vocab_remember.AddUnique(word, index);
          }
          stored.sentences.push_back(l);
        }
      } } catch (const util::EndOfFileException &e) {}
      // <s> and </s> always appear
      Seen &bos = seen_[model.GetVocabulary().BeginSentence()], &eos = seen_[model.GetVocabulary().EndSentence()];
      if (bos.sentences.empty())
        vocab_remember.AddUnique("<s>", model.GetVocabulary().BeginSentence());
      if (eos.sentences.empty())
        vocab_remember.AddUnique("</s>", model.GetVocabulary().EndSentence());
      bos.sentences.resize(l);
      for (unsigned i = 0; i < l; ++i) bos.sentences[i] = i;
      eos.sentences = bos.sentences;

      ngram::Config model_config;
      model_config.write_mmap = "filtered_trie";
      model_config.write_method = ngram::Config::WRITE_MMAP;
      model_config.pointer_bhiksha_bits = 15;
      Building building(vocab_remember.Size(), model.Order(), model_config);
      uint64_t * &vocab_end = building.vocab_.EndHack();
      vocab_end = vocab_remember.Finish(vocab_end, seen_);
      std::cerr << "Wrote vocab words?" << std::endl;
      building.vocab_.Populated();
      std::size_t vocab_size = building.backing_.vocab.size();
      building.backing_.vocab.reset();

      ngram::trie::NodeRange range;
      range.begin = 0;
      range.end = model.GetVocabulary().Bound();

      std::vector<uint64_t> counts(model.Order());
      JustCount counter(&counts[0]);
      Dump<util::ErsatzProgress, JustCount>(model, 1, range, counter);
      for (unsigned char i = 0; i < model.Order(); ++i) {
        std::cout << "ngram " << static_cast<unsigned>(i + 1) << '=' << counts[i] << '\n';
      }

      // Setup output model.
      uint64_t search_size = Building::SearchBackend::Size(counts, model_config);
      util::ResizeOrThrow(building.backing_.file.get(), vocab_size + search_size);
      util::Rolling mem(building.backing_.file.get(), true, 64 << 20, 1024, vocab_size, search_size);
      building.search_.SetupMemory(mem, counts, model_config);

      WriteTrie writer(building);
      Dump<util::ErsatzProgress, WriteTrie>(model, 1, range, writer);
      building.search_.ExternalFinished(model_config, counts[0]);
      building.backing_.search.reset();
      building.backing_.vocab.reset(util::MapOrThrow(vocab_size, true, util::kFileFlags, false, building.backing_.file.get()), vocab_size, util::scoped_memory::MMAP_ALLOCATED);
      building.ExternalFinish(model_config, counts);
    }

  private:
    template <class Progress, class Action> void Dump(ngram::TrieModel &model, const unsigned char order, const ngram::trie::NodeRange range, Action &action) {
      ngram::trie::NodeRange pointer;
      ProbBackoff weights;
      WordIndex word;
      Progress progress(range.end);
      for (uint64_t i = range.begin; i < range.end; ++i, ++progress) {
        model.search_.CheckedRead(order, i, word, weights, pointer);
        if (seen_[word].sentences.empty()) continue;
        sets_.push_back(boost::iterator_range<std::vector<unsigned>::const_iterator>(seen_[word].sentences.begin(), seen_[word].sentences.end()));
        sets_copy_ = sets_;
        if (util::FirstIntersection(sets_copy_)) {
          // Record id, going backwards so it comes out in normal order.
          ids_[KENLM_MAX_ORDER-order] = seen_[word].new_id;
          action(ids_ + KENLM_MAX_ORDER - order, ids_ + KENLM_MAX_ORDER, weights);
          /*out_[order - 1] << weights.prob << '\t' << words_[order - 1];
          for (char w = static_cast<char>(order) - 2; w >= 0; --w) {
            out_[static_cast<unsigned char>(order - 1)] << ' ' << words_[static_cast<unsigned char>(w)];
          }
          out_[order - 1] << '\t' << weights.backoff << '\n';*/
          Dump<NoOpProgress>(model, order + 1, pointer, action);
        }
        sets_.pop_back();
      }
    }

    util::Pool pool_;
    std::vector<Seen> seen_;

    WordIndex ids_[KENLM_MAX_ORDER];

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
