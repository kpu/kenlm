#include "lm/ngram_query.hh"
#include "lm/model.hh"
#include "lm/word_index.hh"
#include "lm/interpolate/enumerate_global_vocab.hh"


#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <map>
#include <iomanip>

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

  bool AllowExtrapolation = true; // if true, params need not sum to one
  bool AllowNegativeParams = true; // if true, params can be negative
  const int ITERATIONS = 20;
  double minstepsize=1.0e-12; // convergence criterion
  int context_size=5; // (context_size+1)-grams considered in perplexity
  double stepdecreasefactor=0.1; // if step unsuccessful

  const int nlambdas = models.size() + (HAS_BIAS ? 1 : 0); // bias + #models
  DVector params = DVector::Constant(nlambdas,1.0/nlambdas); // initialize to sum to 1
  DMatrix N = DMatrix::Constant(nlambdas,nlambdas-1, -1.0/sqrt((nlambdas-1)*(nlambdas-1)+nlambdas-1.0));
  for (unsigned i=0; i<nlambdas-1; ++i)
    N(i,i)= N(i,i)*(1.0-nlambdas);
  // N is nullspace matrix, each column sums to zero

  vector<DVector> paramhistory;

  double bestppl=0.0; // best recorded ppl
  DVector bestgrad = DVector::Zero(nlambdas); // corresp. gradient, feasible direction
  DVector bestparams = DVector::Zero(nlambdas); // corresp. weights
  double stepsize = 1; // dist. from bestparams to params
  double maxbestgradstep=0.0; // max feasible step in grad. direction

  cerr << "++ Parameter training ++" << endl;
  if (AllowExtrapolation)
    cerr << "Allowing extrapolation (sharpening and flattening of individual LM distributions)" << endl;
  else
    cerr << "Interpolating only (not sharpening or flattening individual LM distributions)" << endl;
  if (AllowNegativeParams)
    cerr << "Allowing negative parameters\n" <<
      " (more general but slow and rarely useful\n" <<
      "  -LM with negative weight has probability rankings reversed and is weighted higher than all LMs with positive weights)" << endl;
  else
    cerr << "Not allowing negative parameters (mild assumption, and faster)" << endl;
  cerr << " Maximum number of iterations: " << ITERATIONS << endl;
  cerr << " Minimum step size: " << minstepsize << endl;
  cerr << " Perplexity computed with " << context_size+1 << "-grams" << endl;

  if ((!AllowExtrapolation) && (nlambdas==1))
  {
    // Nothing to optimize. One parameter, and it sums to one.
    cerr << "Training complete. Best weights:" << endl;
    cerr << setprecision(16) << bestparams << endl;
    return;
  }

  for (int iter = 0; iter < ITERATIONS; ++iter) { // iterations
    cerr << "ITERATION " << iter+1 << " (of max " << ITERATIONS << "), step size " << stepsize << " (of min " << minstepsize << "), weights: " << endl;
    cerr << params << endl;

    paramhistory.push_back(params);
    vector<string> context(context_size); // Hard-coded to be 6-gram perplexity
    double ppl = 0.0;
    DVector grad = DVector::Zero(nlambdas);
    DMatrix H = DMatrix::Zero(nlambdas, nlambdas);
    for (unsigned ci = 0; ci < corpus.size(); ++ci) { // sentences in tuning corpus
      const vector<string>& sentence = corpus[ci];
      // pad our beginning context
      std::fill(context.begin(), context.end(), "<s>");
      for (unsigned t = 0; t < sentence.size(); ++t) { // words in sentence
        //std::cerr << "here..." << std::endl;
        DVector feats            = DVector::Zero(nlambdas);
        set_features(context, sentence[t], models, feats); // probs for actual n-gram

        double z = 0.0;
        double maxlogprob=0.0; // Allows us to avoid overflow with negative params
        DVector expectfeats      = DVector::Zero(nlambdas);
        DMatrix expectfeatmatrix = DMatrix::Zero(nlambdas, nlambdas);
        DVector iterfeats = DVector::Zero(nlambdas); // Logically, this should be in the loop's scope
        for (unsigned i = 0; i < vocab.size(); ++i) { // probs over possible n-grams, for normalization
          set_features(context, vocab[i], models, iterfeats);
          double logprob = params.dot(iterfeats);
          if (i==0)
            //maxlogprob=logprob;// more precise, less underflow
            maxlogprob=0.0;// saves operations
          else
            if (logprob>maxlogprob)
            {
              double adjust = exp(maxlogprob-logprob);
              z                *= adjust;
              expectfeats      *= adjust;
              expectfeatmatrix *= adjust;
              maxlogprob=logprob;
            }
          double us = exp(params.dot(iterfeats)-maxlogprob); // measure
          //double us = exp(params.dot(iterfeats)); // measure

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
        //context.push_back(sentence[t]); // old code

        // Perplexity (actually log(perplexity))
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
    //cerr << "ITERATION " << iter << "/" << ITERATIONS << ": log(PPL)=" << ppl << " PPL=" << exp(ppl) << endl;
    cerr << " log(PPL)=" << ppl << " PPL=" << exp(ppl) << endl;
    //cerr << "Input Weights: " << endl;
    //cerr << params << endl;

    if ((ppl<bestppl) || (iter==0))
    {
      // Found a new best
      bestppl=ppl;
      bestparams=params;
      double beststepsize=stepsize;
      if (iter>0)
        cerr << " New best point found, step size " << beststepsize << endl;

      bestgrad=grad;
      DVector deltaparams = DVector::Zero(nlambdas);

      bool reverttograd=false;
      bool solvesuccess=true;

      // Find Newton step
      if (AllowExtrapolation)
      {
        deltaparams = -H.colPivHouseholderQr().solve(grad);
        Eigen::SelfAdjointEigenSolver<DMatrix> eigensolver(H);
        cerr << "Eigenvalues (best if all positive):\n" << eigensolver.eigenvalues() << endl;
        solvesuccess = grad.isApprox(-H*deltaparams);
      }
      else
      {
        bestgrad=N*N.transpose()*bestgrad; // Project gradient to interpolation space

        // need to work in nullspace to maintain unit sum
        DMatrix Hnull = DMatrix::Zero(nlambdas-1, nlambdas-1);

        // Looks like we don't need the three lines below -- we can do it in-line (if we don't want eigenvalues)
        Hnull=N.transpose()*H*N;
        Eigen::SelfAdjointEigenSolver<DMatrix> eigensolver(Hnull);
        cerr << "Eigenvalues (best if all positive):\n" << eigensolver.eigenvalues() << endl;
        deltaparams = -N*Hnull.fullPivHouseholderQr().solve(N.transpose()*grad);
        solvesuccess = (N.transpose()*grad).isApprox(-Hnull*deltaparams);
      }
      // eventually, params = bestparams + deltaparams;
      if (solvesuccess)
      {
        stepsize=0.0;
        for (unsigned i = 0; i<nlambdas; i++)
          stepsize += deltaparams(i)*deltaparams(i);
        stepsize=sqrt(stepsize); // holds length of Newton step
        cerr << "Newton step, length " << stepsize << ": " << endl;
        cerr << deltaparams << endl;

        // Don't let the Newton step get much bigger than last successful step (likely would have to shrink)
        if (stepsize>2.0*beststepsize)
        {
          stepsize=2.0*beststepsize;
          reverttograd=true;
          cerr << "Reverting to gradient, because Newton step is too large." << endl;
        }
      }
      else
      {
        stepsize=2.0*beststepsize;
        reverttograd=true;
        cerr << "Reverting to gradient, because Newton step computation unsuccessful." << endl;
      }
      // Make the gradient unit norm, in feasible search direction.
      if (!AllowNegativeParams)
      {
        // Project gradient to be a feasible search direction
        vector<bool> active(nlambdas,false);
        unsigned numactive=0;
        for (unsigned i = 0; i<nlambdas; i++)
          if ((bestparams(i)==0) && (bestgrad(i)>0)) // Project gradient to inactive constraints
          {
            active[i]=true;
            bestgrad(i)=0.0;// Do this now, in case extrapolation allowed.
            ++numactive;
          }
        if (numactive>0)
        {
          if (!AllowExtrapolation)
          {
            // Project gradient, for activity concerns
            DMatrix tmpN = DMatrix::Constant(nlambdas,nlambdas-1, -1.0/sqrt((nlambdas-numactive-1)*(nlambdas-numactive-1)+nlambdas-numactive-1.0));
            for (unsigned i=0; i<nlambdas-1; ++i)
              tmpN(i,i)= tmpN(i,i)*(1.0-(nlambdas-numactive));
            for (unsigned i=0; i<nlambdas; ++i)
              if (active[i])
                for (unsigned j=0; j<nlambdas-1; ++i)
                  tmpN(i,j)=0;
            bestgrad = -tmpN*tmpN.transpose()*bestgrad; // projected gradient onto unit sum and active set constraints
          }
        }
      }
      double norm=0.0;
      for (unsigned i = 0; i<nlambdas; i++)
        norm += bestgrad(i)*bestgrad(i);
      if (norm!=0)
        bestgrad /= sqrt(norm);
      else
      {
        cerr << " Gradient is zero. Exiting.";
        break;
      }
      cerr << "Gradient, unit length: " << endl;
      cerr << bestgrad << endl;

      // Find max step in gradient direction that remains feasible.
      if (!AllowNegativeParams)
      {
        double limitfraction=0.5; // Not 1: If Newton step is bad, probably will need to reduce later anyway
        for (unsigned i = 0; i<nlambdas; i++)
          if (bestparams(i)-maxbestgradstep*bestgrad(i)<0)
          {
            double tmplimitfraction = bestparams(i)/(bestgrad(i)*maxbestgradstep);
            if (tmplimitfraction<limitfraction)
              limitfraction=tmplimitfraction;
          }
        maxbestgradstep=stepsize*limitfraction;
        cerr << " Max grad step: " << maxbestgradstep << endl;
      }
      else
        maxbestgradstep=stepsize;

      if (!reverttograd)
      {
        if (!AllowNegativeParams)
          for (unsigned i = 0; i<nlambdas; i++)
            if (bestparams(i)+deltaparams(i)<0) // Can't do Newton step. Revert to descent.
              reverttograd=true;
        if (reverttograd)
          cerr << "Reverting to gradient, since Newton step infeasible:" << endl;
      }

      if (reverttograd)
      {
        stepsize=maxbestgradstep;
        deltaparams = -bestgrad * stepsize;
      }

      params=bestparams+deltaparams;
      cerr << "Delta weights, step size " << stepsize << ": " << endl;
      cerr << deltaparams << endl;
      //cerr << "Safeguarded weights: " << endl;
      //cerr << params << endl;
    }
    else
    {
      // Last attempt failed at being better.
      stepsize=std::min(stepdecreasefactor*stepsize,maxbestgradstep); // stepsize reduction factor is empirical
      cerr << "Taking smaller step: " << stepsize << endl;
      params = bestparams - bestgrad * stepsize;
    }
    // Clean them up.
    double sumparams=0.0;
    for (unsigned i = 0; i<nlambdas; i++)
    {
      if (!AllowNegativeParams)
        if (params(i)<1e-12)
          params(i)=0; // snap to zero, for active set and duplicate weights
      sumparams+= params(i);
    }
    if (!AllowExtrapolation)
      params /= sumparams;

    unsigned dedupattempts=0;

    bool duplicateentry=false;
    for (unsigned i=0; i<paramhistory.size(); ++i)
      if (params==paramhistory[i])
        duplicateentry=true;
    while ((duplicateentry) && (dedupattempts<50))
    {
      dedupattempts++;
      cerr << "Duplicate weight found: " << endl;
      cerr << params << endl;
      stepsize*=0.5; // Step in this direction is duplicate, so try again with smaller step
      params = bestparams - stepsize * bestgrad;

      sumparams=0.0;
      for (unsigned i = 0; i<nlambdas; i++)
      {
        if (!AllowNegativeParams)
          if (params(i)<1e-12)
            params(i)=0;
        sumparams+= params(i);
      }
      if (!AllowExtrapolation)
        params /= sumparams;

      duplicateentry=false;
      for (unsigned i=0; i<paramhistory.size(); ++i)
        if (params==paramhistory[i])
          duplicateentry=true;
    }
    if (stepsize<minstepsize)
      break; // No need to make another step

  }

  cerr << "Training complete. Best weights:" << endl;
  cerr << setprecision(16) << bestparams << endl;

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



