/*
Usage example
1) Download from http://www.gwinnup.org/lminterp/train-params-output.tar.bz2
2) then run      perf_enum_gv -t lm.en.dev -m model-a.3.srilm -m model-b.3.srilm -m model-c.3.srilm
 */

#include "lm/ngram_query.hh"
#include "lm/model.hh"
#include "lm/word_index.hh"
#include "lm/interpolate/enumerate_global_vocab.hh"

#include "util/fixed_array.hh"
#include "util/usage.hh"

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>

#include <boost/program_options.hpp>
#include <boost/version.hpp>
#include <boost/foreach.hpp>

#include <Eigen/Eigen>

#include <iostream>
#include <sys/time.h>

inline double deltaTV(const timeval& s, const timeval& e)
{
    return (e.tv_sec - s.tv_sec)*1000.0 + (e.tv_usec - s.tv_usec)/1000.0;
}

typedef struct timeval Wall;
Wall GetWall() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv;
}

typedef Eigen::MatrixXf FMatrix;
typedef Eigen::VectorXf FVector;


bool HAS_BIAS = true;

using namespace lm::ngram;
using namespace lm;

inline void logProb(Model * model, const std::vector<std::string>& ctx, const std::string& word) {

  // Horribly inefficient
  const Vocabulary &vocab = model->GetVocabulary();

  State nextState; //throwaway

  WordIndex word_idx = vocab.Index(word);
  WordIndex context_idx[ctx.size()];

  //reverse context
  for(unsigned int i = 0; i < ctx.size(); i++) {
    context_idx[ctx.size() - 1 - i] = vocab.Index(ctx[i]);
  }
  FullScoreReturn score = model->FullScoreForgotState(context_idx, &(context_idx[ctx.size() -1]), word_idx, nextState);    
}

void set_features(const std::vector<std::string>& ctx,
                  const std::string& word,
                  const std::vector<Model *>& models,
                  FVector& v) {

  for (unsigned i=0; i < models.size(); ++i)
    logProb(models[i], ctx, word);

}

//const util::FixedArray<Model *>& models)
void train_params(
    const std::vector<std::vector<std::string> >& corpus,
    const std::vector<std::string>& vocab,
    const std::vector<Model *>& models) {
  using namespace std;

  vector<string> context(5, "<s>");
  const int ITERATIONS = 10;
  const int nlambdas = models.size(); // #models
  FVector params = FVector::Zero(nlambdas);
  vector<FVector> feats(vocab.size(), params);
  static Wall start,stop;

  for (int iter = 0; iter < ITERATIONS; ++iter) { // iterations
    std::cout << "iteration: " << iter 
              << " corpus size " << corpus.size() 
              << std::endl;
    for (unsigned ci = 0; ci < corpus.size(); ++ci) { // sentences in tuning corpus
      const vector<string>& sentence = corpus[ci];
      context.resize(5);
      for (unsigned t = 0; t < sentence.size(); ++t) { // words in sentence
        std::cout <<  "sentence " << ci << " word " << t << std::endl;
        start = GetWall();
        const string& ref_word_string = sentence[t];
        for (unsigned i = 0; i < vocab.size(); ++i) { // vocab
          set_features(context, vocab[i], models, feats[i]);
        }
        stop = GetWall();
        std::cout << " time elapsed = " << deltaTV(start,stop)  << std::endl;
        context.push_back(ref_word_string);
      }
    }
  }
}

int main(int argc, char** argv) {

  std::string tuning_data;
  std::vector<std::string> lms;

  try {
    namespace po = boost::program_options;
    po::options_description options("train-params");

    options.add_options()
      ("help,h", po::bool_switch(), "Show this help message")
      ("no_bias_term,B", po::bool_switch(), "Do not include a 'bias' feature")
      ("tuning_data,t", po::value<std::string>(&tuning_data), "File to tune perplexity on")
      ("model,m", po::value<std::vector<std::string> >(&lms), "Language models in KenLM format to interpolate");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);

    // Display help
    if(argc == 1 || vm["help"].as<bool>()) {
      std::cerr << options << std::endl;
      return 1;
    }
    if (vm["no_bias_term"].as<bool>())
      HAS_BIAS = false;
    lms = vm["model"].as<std::vector<std::string> >();
    tuning_data = vm["tuning_data"].as<std::string>();
  }
  catch(const std::exception &e) {

    std::cerr << e.what() << std::endl;
    return 1;

  }
  if (lms.size() < 2) {
    std::cerr << "Please specify at least two language model files with -m LM.KLM\n";
    return 1;
  }
  if (tuning_data.empty()) {
    std::cerr << "Please specify tuning set with -t FILE.TXT\n";
    return 1;
  }

  std::map<std::string, int*> vmap;
  util::FixedArray<WordIndex> vm(2);

  //stuff it into the
  EnumerateGlobalVocab * globalVocabBuilder = new EnumerateGlobalVocab(&vmap, lms.size());
  // EnumerateGlobalVocab * globalVocabBuilder = new EnumerateGlobalVocab(vm);

  Config cfg;
  cfg.enumerate_vocab = (EnumerateVocab *) globalVocabBuilder;

  //load models
  //util::FixedArray<Model *> models(lms.size());
  std::vector<Model *> models;
  for(int i=0; i < lms.size(); i++) {
    std::cerr << "Loading LM file: " << lms[i] << std::endl;

    //haaaack
    globalVocabBuilder->SetCurModel(i); //yes this is dumb

    //models[i] = new Model(lms[i].c_str());
    Model * this_model = new Model(lms[i].c_str(), cfg);
    models.push_back( this_model );

  }

  //assemble vocabulary vector
  std::vector<std::string> vocab;
  std::cerr << "Global Vocab Map has size: " << vmap.size() << std::endl;

  std::pair<StringPiece,int *> me;

  for(std::map<std::string, int*>::iterator iter = vmap.begin(); iter != vmap.end(); ++iter) {
    vocab.push_back(iter->first);
  }
  std::cerr << "Vocab vector has size: " << vocab.size() << std::endl;

  //load context sorted ngrams into vector of vectors
  std::vector<std::vector<std::string> > corpus;

  std::cerr << "Loading context-sorted ngrams: " << tuning_data << std::endl;
  std::ifstream infile(tuning_data);

  for(std::string line; std::getline(infile, line); ) {

    std::vector<std::string> words; {

      std::stringstream stream(line);
      std::string word;

      while(stream >> word) {
        words.push_back(word);
      }
    }
    corpus.push_back(words);
  }

  train_params(corpus, vocab, models);

  return 0;
}
