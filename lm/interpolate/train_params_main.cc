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
#include <Eigen/Dense>

//typedef Eigen::MatrixXf FMatrix;
//typedef Eigen::VectorXf FVector;
typedef Eigen::MatrixXd DMatrix;
typedef Eigen::VectorXd DVector;

bool HAS_BIAS = true;

using namespace lm::ngram;
using namespace lm;

inline double logProb(Model * model, const std::vector<std::string>& ctx, const std::string& word) {

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

  double ret = score.prob;
  //std::cerr << "w: " << word << " p: " << ret << std::endl;
  return ret;
}

void set_features(const std::vector<std::string>& ctx,
    const std::string& word,
    const std::vector<Model *>& models,
    DVector& v) {

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
  DVector params = DVector::Constant(nlambdas,1.0/nlambdas); // initialize to sum to 1
  DMatrix N = DMatrix::Constant(nlambdas,nlambdas-1, -1.0/sqrt((nlambdas-1)*(nlambdas-1)+nlambdas-1.0));
  for (unsigned i=0; i<nlambdas-1; ++i)
    N(i,i)= N(i,i)*(1.0-nlambdas);
  // N is nullspace matrix, each column sums to zero
  //cerr << N << endl;

  cerr << "ITERATION 0/" << ITERATIONS << endl;
  cerr << params << endl;

  vector<DVector> paramhistory;
  vector<DVector> deltaparamhistory;
  vector<double> pplhistory;
  vector<double> fractionhistory;

  for (int iter = 0; iter < ITERATIONS; ++iter) { // iterations
    paramhistory.push_back(params);
    vector<string> context(5, "<s>"); // Hard-coded to be 6-gram perplexity
    double ppl = 0.0;
    DVector grad = DVector::Zero(nlambdas);
    DMatrix H = DMatrix::Zero(nlambdas, nlambdas);
    for (unsigned ci = 0; ci < corpus.size(); ++ci) { // sentences in tuning corpus
      const vector<string>& sentence = corpus[ci];
      //context.resize(5);
      for (unsigned t = 0; t < sentence.size(); ++t) { // words in sentence
        //std::cerr << "here..." << std::endl;
        DVector feats            = DVector::Zero(nlambdas);
        set_features(context, sentence[t], models, feats); // probs for actual n-gram

        double z = 0.0;
        DVector expectfeats      = DVector::Zero(nlambdas);
        DMatrix expectfeatmatrix = DMatrix::Zero(nlambdas, nlambdas);
        DVector iterfeats = DVector::Zero(nlambdas); // Logically, this should be in the loop's scope
        for (unsigned i = 0; i < vocab.size(); ++i) { // probs over possible n-grams, for normalization
          set_features(context, vocab[i], models, iterfeats);
          double us = exp(params.dot(iterfeats)); // measure

          z                += us;
          expectfeats      += us * iterfeats;
          expectfeatmatrix += us * (iterfeats*iterfeats.transpose());
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
        ppl  += params.dot(feats) - log(z);
        // Gradient
        grad += feats - expectfeats;
        // Hessian
        H    += -expectfeatmatrix + expectfeats*expectfeats.transpose();
      }
      cerr << ".";
    }
    ppl  *= -1.0/corpus.size();
    // The gradient and Hessian coefficients cancel out, so don't really need to do this, but it's fast.
    grad *= -1.0/corpus.size();
    H    *= -1.0/corpus.size();
    cerr << "ITERATION " << iter << "/" << ITERATIONS << ": log(PPL)=" << ppl << " PPL=" << exp(ppl) << endl;
    cerr << "Input Weights: " << endl;
    cerr << params << endl;
    // Looks like we don't need the three lines below -- we can do it in-line
    DMatrix Hnull = DMatrix::Zero(nlambdas-1, nlambdas-1);
    Hnull=N.transpose()*H*N;
    Eigen::SelfAdjointEigenSolver<DMatrix> eigensolver(Hnull);
    cerr << "Eigenvalues:\n" << eigensolver.eigenvalues() << endl;
    DVector deltaparams = -N*Hnull.colPivHouseholderQr().solve(N.transpose()*grad);
    //DVector deltaparams = -N*(N.transpose()*H*N).colPivHouseholderQr().solve(N.transpose()*grad);

    int reverttograd=0;
    for (unsigned i = 0; i<nlambdas; i++)
      if (params(i)+deltaparams(i)<0) // Can't do Newton step. Revert to descent.
        reverttograd=1;
    if (reverttograd==1)
    {
      cerr << "Reverting to gradient, since Newton step infeasible." << endl;
      deltaparams = -N*N.transpose()*grad; // projected gradient onto unit sum constraint
      double norm=0.0;
      for (unsigned i = 0; i<nlambdas; i++)
        if ((params(i)==0) && (deltaparams(i)<0)) // Project gradient to inactive constraints
        {
          // Unfortunate if this happens, since it breaks the nullspace conditions
          // Unit sum will be enforced crudely, later. TODO: improve, if it matters.
          deltaparams(i)=0; 
          cerr << "Projecting gradient to active constraint, weight " << i << endl;
        }
        else
          norm += deltaparams(i)*deltaparams(i);
      if (norm>0)
        deltaparams /= sqrt(norm);
    }

    cerr << "Delta weights: " << endl;
    cerr << deltaparams << endl;
    cerr << "Unsafeguarded weights (latest): " << endl;
    cerr << params+deltaparams << endl;

    // Collect history of function value, step, and smallest tried step size
    deltaparamhistory.push_back(deltaparams);
    pplhistory.push_back(ppl);
    fractionhistory.push_back(-1); // Signifies no step yet taken.

    unsigned minindex=pplhistory.size()-1;
    for (unsigned i=0; i<pplhistory.size()-1; ++i)
      if (pplhistory[i]<pplhistory[minindex])
        minindex=i;
    cerr << " Stepping from iteration " << minindex << endl;

    if (fractionhistory[minindex]>0)
      fractionhistory[minindex]*=0.5; // Step in this direction failed to decrease last time, so try again with smaller step
    else
    {
      // First time to try a step. Make as large a feasible step as possible
      fractionhistory[minindex]=1;
      // We should be done (negative weights don't screw up the math), but they do screw
      // up the numerics. We now explicitly disallow them:
      for (unsigned i = 0; i<nlambdas; i++)
        if (params(i)+deltaparams(i)<0)
        {
          double tmplimitfraction = params(i)/-deltaparams(i);
          if (tmplimitfraction<fractionhistory[minindex])
            fractionhistory[minindex]=tmplimitfraction;
        }
    }

    params = paramhistory[minindex] + fractionhistory[minindex]*deltaparamhistory[minindex];

    double sumparams=0.0;
    for (unsigned i = 0; i<nlambdas; i++)
    {
      if (params(i)<1e-12)
        params(i)=0;
      sumparams+= params(i);
    }
    params /= sumparams;

    int duplicateentry=0;
    for (unsigned i=0; i<pplhistory.size(); ++i)
      if (params==paramhistory[i])
        duplicateentry=1;
    if (duplicateentry==1)
    {
      cerr << "Duplicate weight found! " << endl;
      fractionhistory[minindex]*=0.5; // Step in this direction is duplicate, so try again with smaller step
      params = paramhistory[minindex] + fractionhistory[minindex]*deltaparamhistory[minindex];
      sumparams=0.0;
      for (unsigned i = 0; i<nlambdas; i++)
      {
        if (params(i)<1e-12)
          params(i)=0;
        sumparams+= params(i);
      }
      params /= sumparams;
    }

    cerr << "Safeguarded weights: " << endl;
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



