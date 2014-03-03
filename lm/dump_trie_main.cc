#include <fstream>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/stream.hpp>

#include "lm/config.hh"
#include "lm/model.hh"
#include "util/fake_ofstream.hh"
#include "util/file.hh"
#include "util/ersatz_progress.hh"
#include "util/multi_intersection.hh"
#include "util/tokenize_piece.hh"

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

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
  private:
    typedef std::vector<boost::iterator_range<std::vector<unsigned>::const_iterator> > Sets;
    typedef std::vector<StringPiece> Words;
  
  public:
    DumpTrie() {}

    void Dump(lm::ngram::ArrayTrieModel &model, const std::string &base, const std::string &vocab, int threads) {
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
      // <unk> will always appear.  But internal <unk> will only appear in matching sentences.
      Seen &unk = seen_[model.GetVocabulary().NotFound()];
      unk.value = "<unk>";
      // Force <unk> to appear in the very last sentence.
      unk.sentences.push_back(l + 1);

      ngram::trie::NodeRange range;
      range.begin = 0;
      range.end = model.GetVocabulary().Bound();
      for (unsigned char i = 0; i < model.Order(); ++i) {
        count_[i] = 0;
        //files_[i].reset(util::CreateOrThrow((base + boost::lexical_cast<std::string>(static_cast<unsigned int>(i) + 1)).c_str()));
        std::ofstream outStream;
        outStream.open((base + boost::lexical_cast<std::string>(static_cast<unsigned int>(i) + 1) + ".gz").c_str());
        
        out_[i].push(boost::iostreams::gzip_compressor());
        out_[i].push(outStream);
        
        //out_[i].SetFD(files_[i].get());
      }
      
      std::vector<boost::thread*> threadSet;
      util::ErsatzProgress* progress = new util::ErsatzProgress(range.end);      
      for(int i = 0; i < threads; ++i) {
        threadSet.push_back(new boost::thread(&DumpTrie::Dump<util::ErsatzProgress>,
                             this, &model, 1, range, Sets(), Words(KENLM_MAX_ORDER),
                             i, threads, progress));
      }
      
      for(int i = 0; i < threads; ++i) {
        threadSet[i]->join();
        delete threadSet[i];
      }
      delete progress;
      
      for (unsigned char i = 0; i < model.Order(); ++i) {
        std::cout << "ngram " << static_cast<unsigned>(i + 1) << '=' << count_[i] << '\n';
      }
    }

  private:
    template <class Progress> void Dump(ngram::ArrayTrieModel *model, const unsigned char order,
                                        const ngram::trie::NodeRange range,
                                        Sets &sets, Words &words,
                                        int threadNo, int totalThreads,
                                        Progress* progress) {
      ngram::trie::NodeRange pointer;
     
      ProbBackoff weights;
      WordIndex word;

      int shift = order > 1 ? 0 : threadNo;
      int step  = order > 1 ? 1 : totalThreads;
      for (uint64_t i = range.begin + shift; i < range.end; i += step, ++(*progress)) {
        model->search_.CheckedRead(order, i, word, weights, pointer);
     
        if (seen_[word].value.empty()) continue;

        sets.push_back(boost::iterator_range<std::vector<unsigned>::const_iterator>(seen_[word].sentences.begin(), seen_[word].sentences.end()));
        Sets sets_copy = sets;
        if (util::FirstIntersection(sets_copy))
        {
          {
            boost::mutex::scoped_lock lock(mutex_);
            words[order - 1] = seen_[word].value;
            ++count_[order - 1];
            out_[order - 1] << weights.prob << '\t' << words[order - 1];
            for (char w = static_cast<char>(order) - 2; w >= 0; --w) {
              out_[static_cast<unsigned char>(order - 1)] << ' ' << words[static_cast<unsigned char>(w)];
            }
            out_[order - 1] << '\t' << weights.backoff << '\n';
          }
          
          NoOpProgress noop(pointer.end);
          Dump<NoOpProgress>(model, order + 1, pointer, sets, words,
                             threadNo, totalThreads, &noop);
        }
        sets.pop_back();
      }
    }

    util::Pool pool_;
    std::vector<Seen> seen_;
    util::scoped_fd files_[KENLM_MAX_ORDER];
    boost::iostreams::filtering_ostream out_[KENLM_MAX_ORDER];

    uint64_t count_[KENLM_MAX_ORDER];
    boost::mutex mutex_;

};
} // namespace

int main(int argc, char *argv[]) {
  if (argc != 4 && argc != 5) {
    std::cerr << "Usage: " << argv[0] << " trie output_base vocab [threads]" << std::endl;
    return 1;
  }

  lm::ngram::Config config;
  config.load_method = util::LAZY;
  
  lm::ngram::ArrayTrieModel model(argv[1], config); 
  lm::DumpTrie dumper;
  
  int threads = 1;
  if(argc == 5)
    threads = atoi(argv[4]);
              
  dumper.Dump(model, argv[2], argv[3], threads);
  return 0;
}
