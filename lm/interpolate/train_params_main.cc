#include "lm/ngram_query.hh"
#include "lm/model.hh"
#include "lm/word_index.hh"
#include "lm/interpolate/enumerate_global_vocab.hh"


#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>

#include <boost/program_options.hpp>
#include <boost/version.hpp>
#include <boost/foreach.hpp>

#include "util/fixed_array.hh"

#include <Eigen/Eigen>

typedef Eigen::MatrixXf FMatrix;
typedef Eigen::VectorXf FVector;

bool HAS_BIAS = true;

using namespace lm::ngram;
using namespace lm;

inline float logProb(Model * model, const std::vector<std::string>& ctx, const std::string& word) {

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

  float ret = score.prob;
  //std::cerr << "w: " << word << " p: " << ret << std::endl;
  return ret;
}

void set_features(const std::vector<std::string>& ctx,
                  const std::string& word,
                  const std::vector<Model *>& models,
                  FVector& v) {

  //std::cerr << "setting feats for " << word << std::endl;

  if (HAS_BIAS) {
    v(0) = 1;
    for (unsigned i=0; i < models.size(); ++i)
      v(i + 1) = logProb(models[i], ctx, word);
  } else {
    for (unsigned i=0; i < models.size(); ++i)
      v(i) = logProb(models[i], ctx, word);
  }
}

void translate_input(
    const std::vector<std::vector<std::string> >& corpus,
    const std::vector<std::string>& gvocab,
    const std::vector<Model *>& models,
    std::vector<std::vector<std::vector<WordIndex> > >&translated_corpus,
    std::vector<std::vector<WordIndex> >&translated_vocab
  ) {
  translated_corpus.resize(models.size());
  translated_vocab.resize(models.size());
  for (unsigned mn=0; mn < models.size(); ++mn) { // models 

    const Vocabulary &vocab = models[mn]->GetVocabulary();

    for (unsigned i = 0; i < gvocab.size(); ++i) {
      translated_vocab[mn].push_back(vocab.Index(gvocab[i]));
    }
    
    translated_corpus[mn].resize(corpus.size());
    for (unsigned ci = 0; ci < corpus.size(); ++ci) { // sentences in tuning corpus
      const std::vector<std::string>& sentence = corpus[ci];
      for (int t = sentence.size() -1; t >= 0; --t) { // words in sentence
        translated_corpus[mn][ci].push_back(vocab.Index(sentence[t]));
      }
      for (int i=0; i<5; ++i) {
        translated_corpus[mn][ci].push_back(vocab.Index("<s>"));
      }
    }
  }
}
      

void train_params_fast(
    const std::vector<std::vector<std::string> >& corpus,
    const std::vector<std::string>& vocab,
    const std::vector<Model *>& models) {
  using namespace std;

  // model / sentence / words in sentence in reverse order with <s> padding 
  std::vector<std::vector<std::vector<WordIndex> > > t_corpus;
  std::vector<std::vector<WordIndex> > t_vocab;
  translate_input(corpus, vocab, models, t_corpus, t_vocab);



  const int ITERATIONS = 10;
  const int nlambdas = models.size() + (HAS_BIAS ? 1 : 0); // bias + #models
  FVector params = FVector::Zero(nlambdas);
  vector<FVector> feats(vocab.size(), params);
  vector<float> us(vocab.size(), 0);
  vector<float> ps(vocab.size(), 0);
  FVector grad = FVector::Zero(nlambdas);
  FMatrix H = FMatrix::Zero(nlambdas, nlambdas);
  FVector ef = FVector::Zero(nlambdas);
  for (int iter = 0; iter < ITERATIONS; ++iter) { // iterations
    grad.setZero();
    H.setZero();
    double loss = 0;
    unsigned numchars = 0;
    for (unsigned ci = 0; ci < corpus.size(); ++ci) { // sentences in tuning corpus
      const vector<string>& sentence = corpus[ci];
      double z = 0;
      for (int t = sentence.size() -1 ; t >=0; --t) { // words in sentence
        ++numchars;
        int ref_word = 0;
        for (unsigned i = 0; i < vocab.size(); ++i) { // vocab
          // set_features(context, vocab[i], models, feats[i]);
          for (unsigned j=0; j < models.size(); ++j) {
            // NOTE: reference ---- WordIndex word_idx = t_corpus[j][ci][t];
            WordIndex word_idx = t_vocab[j][i];
            State nextState; //throwaway
            FullScoreReturn score = models[j]->FullScoreForgotState(&(t_corpus[j][ci][t]), &(t_corpus[j][ci][t+5]), word_idx, nextState);
            feats[i](j) = score.prob;
            // feats[i](j) = logProb(models[j], ctx, word);
          }
          
          us[i] = params.dot(feats[i]);
          z += exp(double(us[i]));
        }
	//std::cerr << "there..." << std::endl;
        const float logz = log(z);

        // expected feature values
        ef.setZero();
        for (unsigned i = 0; i < vocab.size(); ++i) {
          ps[i] = expf(us[i] - logz);
          ef += ps[i] * feats[i];
        }
        loss -= log(ps[ref_word]);
        const FVector& reffeats = feats[ref_word];
        grad += ef - reffeats;

        // Hessian
        for (unsigned i = 0; i < vocab.size(); ++i)
          H.noalias() += ps[i] * feats[i] * feats[i].transpose() -
                         ps[i] * feats[i] * ef.transpose();

        // this should just be the state for each model
      }
      cerr << ".";
    }
    cerr << "ITERATION " << (iter + 1) << ": PPL=" << exp(loss / numchars) << endl;
    params = H.colPivHouseholderQr().solve(grad);
    cerr << params << endl;
  }
}
          
  


//const util::FixedArray<Model *>& models)
void train_params(
    const std::vector<std::vector<std::string> >& corpus,
    const std::vector<std::string>& vocab,
    const std::vector<Model *>& models) {
  using namespace std;

  vector<string> context(5, "<s>");
  const int ITERATIONS = 10;
  const int nlambdas = models.size() + (HAS_BIAS ? 1 : 0); // bias + #models
  FVector params = FVector::Zero(nlambdas);
  vector<FVector> feats(vocab.size(), params);
  vector<float> us(vocab.size(), 0);
  vector<float> ps(vocab.size(), 0);
  FVector grad = FVector::Zero(nlambdas);
  FMatrix H = FMatrix::Zero(nlambdas, nlambdas);
  FVector ef = FVector::Zero(nlambdas);
  for (int iter = 0; iter < ITERATIONS; ++iter) { // iterations
    grad.setZero();
    H.setZero();
    double loss = 0;
    unsigned numchars = 0;
    for (unsigned ci = 0; ci < corpus.size(); ++ci) { // sentences in tuning corpus
      const vector<string>& sentence = corpus[ci];
      std::fill(context.begin(), context.end(), "<s>");
      for (unsigned t = 0; t < sentence.size(); ++t) { // words in sentence
        ++numchars;
        const string& ref_word_string = sentence[t];
        int ref_word = 0; // TODO
        double z = 0;
	//std::cerr << "here..." << std::endl;
        for (unsigned i = 0; i < vocab.size(); ++i) { // vocab
          set_features(context, vocab[i], models, feats[i]);
          us[i] = params.dot(feats[i]);
          z += exp(double(us[i]));
        }
	//std::cerr << "there..." << std::endl;
        context.push_back(ref_word_string);
        const float logz = log(z);

        // expected feature values
        ef.setZero();
        for (unsigned i = 0; i < vocab.size(); ++i) {
          ps[i] = expf(us[i] - logz);
          ef += ps[i] * feats[i];
        }
        loss -= log(ps[ref_word]);
        const FVector& reffeats = feats[ref_word];
        grad += ef - reffeats;

        // Hessian
        for (unsigned i = 0; i < vocab.size(); ++i)
          H.noalias() += ps[i] * feats[i] * feats[i].transpose() -
                         ps[i] * feats[i] * ef.transpose();

        // this should just be the state for each model
      }
      cerr << ".";
    }
    cerr << "ITERATION " << (iter + 1) << ": PPL=" << exp(loss / numchars) << endl;
    params = H.colPivHouseholderQr().solve(grad);
    cerr << params << endl;
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

  //Growable vocab here
  //GrowableVocab gvoc(100000); //dummy default

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

  train_params_fast(corpus, vocab, models);

  return 0;
}



