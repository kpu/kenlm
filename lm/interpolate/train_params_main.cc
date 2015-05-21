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
typedef Eigen::MatrixXd DMatrix;
typedef Eigen::VectorXd DVector;

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

//const util::FixedArray<Model *>& models)
void train_params(
    const std::vector<std::vector<std::string> >& corpus,
    const std::vector<std::string>& vocab,
    const std::vector<Model *>& models) {
  using namespace std;

  const int ITERATIONS = 10;
  const int nlambdas = models.size() + (HAS_BIAS ? 1 : 0); // bias + #models
  FVector params = FVector::Constant(nlambdas,1.0/nlambdas); // initialize to sum to 1
  FMatrix N = FMatrix::Constant(nlambdas,nlambdas-1, -1.0/sqrt((nlambdas-1)*(nlambdas-1)+nlambdas-1.0));
  for (unsigned i=0; i<nlambdas-1; ++i)
    N(i,i)= N(i,i)*(1.0-nlambdas);
  // N is nullspace matrix, each column sums to zero

  for (int iter = 0; iter < ITERATIONS; ++iter) { // iterations
    FVector sumfeats = FVector::Zero(nlambdas);
    FVector expectfeats = FVector::Zero(nlambdas);
    FMatrix expectfeatmatrix = FMatrix::Zero(nlambdas, nlambdas);
    FVector grad = FVector::Zero(nlambdas);
    FMatrix H = FMatrix::Zero(nlambdas, nlambdas);
    double loss = 0;
    vector<string> context(5, "<s>"); // Hard-coded to be 6-gram perplexity
    for (unsigned ci = 0; ci < corpus.size(); ++ci) { // sentences in tuning corpus
      const vector<string>& sentence = corpus[ci];
      //context.resize(5);
      for (unsigned t = 0; t < sentence.size(); ++t) { // words in sentence
        double z = 0;
        //std::cerr << "here..." << std::endl;
        FVector feats = FVector::Zero(nlambdas);

        set_features(context, sentence[t], models, feats); // 

        // Logically, these next two should be in the loop's scope,
        // but let's just declare them once. 
        FVector iterfeats = FVector::Zero(nlambdas);
        double us;
        for (unsigned i = 0; i < vocab.size(); ++i) { // vocab loop
          set_features(context, vocab[i], models, iterfeats);
          us = exp(double(params.dot(iterfeats))); // measure
          z += us;
          expectfeats   += iterfeats * us;
          expectfeatmatrix += (iterfeats*iterfeats.transpose()) * us;
        }
        expectfeats      /= z; // Expectation
        expectfeatmatrix /= z; // Expectation
        //std::cerr << "there..." << std::endl;

        // This should add sentence[t] to the end of the context, removing the oldest word from the front
        // if needed to keep maximum of (n-1) words (when n-grams are considered in perplexity).
        for (unsigned i = 0; i<context.size()-1; ++i)
          context[i]=context[i+1];
        context[context.size()-1]=sentence[t];
        //context.push_back(sentence[t]);

        // Perplexity
        loss += -log(z) + double(params.dot(sumfeats));
        // Gradient
        grad += feats - expectfeats;
        // Hessian
        H    += -expectfeatmatrix + expectfeats*expectfeats.transpose();
      }
      cerr << ".";
    }
    loss *= -1.0/corpus.size();
    //grad *= -1.0/corpus.size();
    //H    *= -1.0/corpus.size();
    cerr << "ITERATION " << (iter + 1) << ": PPL=" << exp(loss) << endl;
    // Looks like we don't need the three lines below -- we can do it in-line
    //FMatrix Hnull = FMatrix::Zero(nlambdas-1, nlambdas-1);
    //Hnull=N.transpose()*H*N;
    //params += -N*Hnull.colPivHouseholderQr().solve(N.transpose()*grad);
    params += -N*(N.transpose()*H*N).colPivHouseholderQr().solve(N.transpose()*grad);
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

  //no comment
  std::map<std::string, int*> vmap;

  //stuff it into the 
  EnumerateGlobalVocab * globalVocabBuilder = new EnumerateGlobalVocab(&vmap, lms.size());

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
  std::ifstream infile(tuning_data.c_str());

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



