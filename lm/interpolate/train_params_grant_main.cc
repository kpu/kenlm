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

// typedef Eigen::MatrixXf FMatrix;
// typedef Eigen::VectorXf FVector;
typedef Eigen::MatrixXd DMatrix;
typedef Eigen::VectorXd DVector;

bool HAS_BIAS = true;

using namespace lm::ngram;
using namespace lm;

inline double logProb(Model *model, const std::vector<std::string> &ctx,
                      WordIndex word_idx) {
  // Horribly inefficient
  const Vocabulary &vocab = model->GetVocabulary();

  State nextState;  // throwaway

  WordIndex context_idx[ctx.size()];

  // reverse context
  for (std::size_t i = 0; i < ctx.size(); i++) {
    context_idx[ctx.size() - 1 - i] = vocab.Index(ctx[i]);
  }

  FullScoreReturn score = model->FullScoreForgotState(
      context_idx, &(context_idx[ctx.size() - 1]), word_idx, nextState);

  double ret = score.prob;
  // std::cerr << "w: " << word << " p: " << ret << std::endl;
  return ret;
}

inline double logProb(Model *model, double unkprob,
                      const std::vector<std::string> &ctx,
                      const std::string &word) {
  // Horribly inefficient
  const Vocabulary &vocab = model->GetVocabulary();

  WordIndex word_idx = vocab.Index(word);
  if (word_idx == lm::kUNK) return unkprob;

  return logProb(model, ctx, word_idx);
}

void set_features(const std::vector<std::string> &ctx, const std::string &word,
                  const std::vector<Model *> &models,
                  const std::vector<double> &unkprobs, DVector &v) {
  if (HAS_BIAS) {
    v(0) = 1;
    for (std::size_t i = 0; i < models.size(); ++i)
      v(i + 1) = logProb(models[i], unkprobs[i], ctx, word);
  } else {
    for (std::size_t i = 0; i < models.size(); ++i)
      v(i) = logProb(models[i], unkprobs[i], ctx, word);
  }
}

void train_params(const std::vector<std::vector<std::string> > &corpus,
                  const std::vector<std::string> &vocab,
                  const std::vector<Model *> &models) {
  using namespace std;

  // A safeguarded Newton's method to find optimum parameters.
  // Reverts to steepest-descent linesearch if Newton step does not improve
  // objective.
  //
  // Two Boolean variables below are used to "AllowExtrapolation" and
  // "AllowNegativeParams".

  bool AllowExtrapolation = true;   // if true, params need not sum to one
  bool AllowNegativeParams = true;  // if true, params can be negative
  const int ITERATIONS = 20;
  double minstepsize = 1.0e-9;  // convergence criterion
  int context_size = 5;  // (context_size+1)-grams considered in perplexity
  double stepdecreasefactor = 0.1;        // if step unsuccessful
  double initstepsize = 1.0;              // Initial step size
  std::size_t linesinstartercorpus = 12;  // The first few lines are tuned
                                          // first, to find basin of attraction
                                          // for Newton
  // bias + #models
  const std::size_t nlambdas = models.size() + (HAS_BIAS ? 1 : 0);

  // initialize to sum to 1
  DVector params = DVector::Constant(nlambdas, 1.0 / nlambdas);
  DMatrix N = DMatrix::Constant(
      nlambdas, nlambdas - 1,
      -1.0 / sqrt((nlambdas - 1) * (nlambdas - 1) + nlambdas - 1.0));
  for (unsigned i = 0; i < nlambdas - 1; ++i)
    N(i, i) = N(i, i) * (1.0 - nlambdas);
  // N is nullspace matrix, each column sums to zero

  cerr << setprecision(16) << "++ Parameter training ++" << endl;
  if (AllowExtrapolation)
    cerr << " Allowing extrapolation (sharpening and flattening of individual "
            "LM distributions)" << endl;
  else
    cerr << " Interpolating only (not sharpening or flattening individual LM "
            "distributions)" << endl;
  if (AllowNegativeParams)
    cerr << " Allowing negative parameters\n"
         << "  (more general but slow and rarely useful\n"
         << "   -LM with negative weight has probability rankings reversed and "
            "is weighted more highly than all LMs with positive weights)"
         << endl;
  else
    cerr << "Not allowing negative parameters (mild assumption, and faster)"
         << endl;
  cerr << " Maximum number of iterations: " << ITERATIONS << endl;
  cerr << " Minimum step size: " << minstepsize << endl;
  cerr << " Perplexity computed with " << context_size + 1 << "-grams" << endl;

  if ((!AllowExtrapolation) && (nlambdas == 1)) {
    // Nothing to optimize. One parameter, and it sums to one.
    cerr << "Training complete. Best weights:" << endl;
    cerr << setprecision(16) << 1.0 << endl;
    return;
  }

  // Smart initialization of full tuning by tuning on smaller set first
  vector<std::size_t> linestotune;
  if (linesinstartercorpus < corpus.size())
    linestotune.push_back(linesinstartercorpus);
  linestotune.push_back(corpus.size());

  for (std::size_t setiter = 0; setiter < linestotune.size(); ++setiter) {
    cerr << " Now tuning the first " << linestotune[setiter] << " lines"
         << endl;

    vector<DVector> paramhistory;
    double bestppl = 0.0;                          // best recorded ppl
    DVector bestgrad = DVector::Zero(nlambdas);    // corresp. gradient,
                                                   // feasible direction
    DVector bestparams = DVector::Zero(nlambdas);  // corresp. weights
    double maxbestgradstep = 0.0;    // max feasible step in grad. direction
    double stepsize = initstepsize;  // Initial step size

    for (int iter = 0; iter < ITERATIONS; ++iter) {  // iterations
      cerr << "ITERATION " << iter + 1 << " (of max " << ITERATIONS
           << "), step size " << stepsize << " (of min " << minstepsize
           << "), weights: " << endl;
      cerr << params << endl;

      paramhistory.push_back(params);
      // Hard-coded to be 6-gram perplexity
      vector<string> context(context_size, "<s>");
      double ppl = 0.0;
      DVector grad = DVector::Zero(nlambdas);
      DMatrix H = DMatrix::Zero(nlambdas, nlambdas);
      cerr << "o";

      std::vector<double> unkprobs(models.size());
      // for each sentence in tuning corpus
      for (std::size_t ci = 0; ci < linestotune[setiter]; ++ci) {
        const vector<string> &sentence = corpus[ci];
        // pad our beginning context
        std::fill(context.begin(), context.end(), "<s>");

        // for each word in sentence
        for (std::size_t t = 0; t < sentence.size(); ++t) {
          // fill in unk probabilities for this context, to avoid having to
          // look them up redundantly later
          for (std::size_t mi = 0; mi < models.size(); ++mi) {
            unkprobs[mi] = logProb(models[mi], context, lm::kUNK);
          }

          DVector feats = DVector::Zero(nlambdas);
          // probs for actual n-gram
          set_features(context, sentence[t], models, unkprobs, feats);

          double z = 0.0;
          double maxlogprob = 0.0;  // Allows us to avoid overflow with
                                    // negative params
          DVector expectfeats = DVector::Zero(nlambdas);
          DMatrix expectfeatmatrix = DMatrix::Zero(nlambdas, nlambdas);
          // Logically, this should be in the loop's scope
          DVector iterfeats = DVector::Zero(nlambdas);

          // probs over possible n-grams, for normalization
          for (std::size_t i = 0; i < vocab.size(); ++i) {
            set_features(context, vocab[i], models, unkprobs, iterfeats);
            double logprob = params.dot(iterfeats);
            if (i == 0) {
              // maxlogprob=logprob;// more precise, less underflow
              maxlogprob = 0.0;  // reduces number of updates
            } else if (logprob > maxlogprob) {
              // Adjust all old values to new scaling
              double adjust = exp(maxlogprob - logprob);
              z *= adjust;
              expectfeats *= adjust;
              expectfeatmatrix *= adjust;
              maxlogprob = logprob;
            }
            double us = exp(params.dot(iterfeats) - maxlogprob);  // measure

            z += us;
            expectfeats += us * iterfeats;
            expectfeatmatrix += us * (iterfeats * iterfeats.transpose());
          }
          expectfeats /= z;       // Expectation
          expectfeatmatrix /= z;  // Expectation

          // Add sentence[t] to the end of the context
          context[0] = sentence[t];
          std::rotate(context.begin(), context.begin() + 1, context.end());

          // Perplexity (actually log(perplexity))
          ppl += params.dot(feats) - log(z);
          // Gradient
          grad += feats - expectfeats;
          // Hessian
          H += -expectfeatmatrix + expectfeats * expectfeats.transpose();
        }
        cerr << ".";
      }
      ppl *= -1.0 / corpus.size();
      // The gradient and Hessian coefficients cancel out, so don't really need
      // to do this, but it's fast.
      grad *= -1.0 / corpus.size();
      H *= -1.0 / corpus.size();
      cerr << " log(PPL)=" << ppl << " PPL=" << exp(ppl) << endl;

      // Use results to determine next params to evaluate
      if ((ppl < bestppl) || (iter == 0)) {
        // Found a new best
        bestppl = ppl;
        bestparams = params;
        double beststepsize = stepsize;
        if (iter > 0)
          cerr << " New best point found, step size " << beststepsize << endl;
        else
          cerr << " New best point found" << endl;

        bestgrad = grad;
        DVector deltaparams = DVector::Zero(nlambdas);

        bool reverttograd = false;

        {
          double gradnorm = 0.0;
          double solvenorm = 0.0;
          double errnorm = 0.0;
          // Find Newton step
          if (AllowExtrapolation) {
            deltaparams = -H.colPivHouseholderQr().solve(grad);
            Eigen::SelfAdjointEigenSolver<DMatrix> eigensolver(H);
            cerr << "Eigenvalues (negative values should be negligible):\n"
                 << eigensolver.eigenvalues() << endl;
            gradnorm = grad.norm();
            solvenorm = (H * deltaparams).norm();
            errnorm = (grad + H * deltaparams).norm();
          } else {
            // Project gradient to interpolation space
            bestgrad = N * N.transpose() * bestgrad;

            // need to work in nullspace to maintain unit sum
            DMatrix Hnull = DMatrix::Zero(nlambdas - 1, nlambdas - 1);

            // Looks like we don't need the three lines below -- we can do it
            // in-line (if we don't want eigenvalues)
            Hnull = N.transpose() * H * N;
            Eigen::SelfAdjointEigenSolver<DMatrix> eigensolver(Hnull);
            cerr << "Eigenvalues (best if all positive):\n"
                 << eigensolver.eigenvalues() << endl;
            deltaparams =
                -N * Hnull.fullPivHouseholderQr().solve(N.transpose() * grad);
            gradnorm = (N.transpose() * grad).norm();
            solvenorm = (Hnull * deltaparams).norm();
            errnorm = (N.transpose() * grad + Hnull * deltaparams).norm();
          }
          // eventually, params = bestparams + deltaparams;
          cerr << " Error norm " << errnorm << ", gradient norm " << gradnorm
               << ", solution norm " << solvenorm << endl;
          // Check for numerical errors. Don't trust Newton step if they are too
          // big.
          if (errnorm < 1e-12 * std::max(1.0, std::min(gradnorm, solvenorm))) {
            stepsize = 0.0;
            for (std::size_t i = 0; i < nlambdas; i++)
              stepsize += deltaparams(i) * deltaparams(i);
            stepsize = sqrt(stepsize);  // holds length of Newton step
            cerr << "Newton step, length " << stepsize << ": " << endl;
            cerr << deltaparams << endl;

            // Don't let the Newton step get much bigger than last successful
            // step (likely would have to shrink later, anyway)
            if (stepsize > 2.0 * beststepsize) {
              stepsize = 1.5 * beststepsize;
              reverttograd = true;
              cerr << "Reverting to gradient, because Newton step is too large."
                   << endl;
            }
          } else {
            stepsize = 1.5 * beststepsize;
            reverttograd = true;
            cerr << "Reverting to gradient, because Newton step computation "
                    "unsuccessful." << endl;
          }
          // Make the gradient unit norm, in feasible search direction.
          if (!AllowNegativeParams) {
            // Project gradient to be a feasible search direction
            vector<bool> active(nlambdas, false);
            std::size_t numactive = 0;
            for (std::size_t i = 0; i < nlambdas; i++) {
              // Project gradient to inactive constraints
              if ((bestparams(i) == 0) && (bestgrad(i) > 0)) {
                active[i] = true;
                bestgrad(i) = 0.0;  // Do this now, in case extrapolation
                                    // allowed.
                ++numactive;
              }
            }
            if (numactive > 0) {
              if (!AllowExtrapolation) {
                // Project gradient, for activity concerns
                DMatrix tmpN = DMatrix::Constant(
                    nlambdas, nlambdas - 1,
                    -1.0 / sqrt((nlambdas - numactive - 1) *
                                    (nlambdas - numactive - 1) +
                                nlambdas - numactive - 1.0));

                for (std::size_t i = 0; i < nlambdas - 1; ++i)
                  tmpN(i, i) = tmpN(i, i) * (1.0 - (nlambdas - numactive));

                for (std::size_t i = 0; i < nlambdas; ++i) {
                  if (active[i]) {
                    for (std::size_t j = 0; j < nlambdas - 1; ++i) {
                      tmpN(i, j) = 0;
                    }
                  }
                }

                // projected gradient onto unit sum and active set constraints
                bestgrad = -tmpN * tmpN.transpose() * bestgrad;
              }
            }
          }
        }
        double norm = 0.0;
        for (std::size_t i = 0; i < nlambdas; i++)
          norm += bestgrad(i) * bestgrad(i);
        if (norm != 0) {
          bestgrad /= sqrt(norm);
        } else {
          cerr << " Gradient is zero. Exiting.";
          break;
        }
        cerr << "Gradient, unit length: " << endl;
        cerr << bestgrad << endl;

        // Find max step in gradient direction that remains feasible.
        if (!AllowNegativeParams) {
          double limitfraction = 0.5;  // Not 1: If Newton step is bad, probably
                                       // will need to reduce later anyway
          for (std::size_t i = 0; i < nlambdas; i++) {
            if (bestparams(i) - maxbestgradstep * bestgrad(i) < 0) {
              double tmplimitfraction =
                  bestparams(i) / (bestgrad(i) * maxbestgradstep);
              if (tmplimitfraction < limitfraction)
                limitfraction = tmplimitfraction;
            }
          }
          maxbestgradstep = stepsize * limitfraction;
          cerr << " Max grad step: " << maxbestgradstep << endl;
        } else {
          maxbestgradstep = stepsize;
        }

        if (!reverttograd) {
          if (!AllowNegativeParams) {
            for (std::size_t i = 0; i < nlambdas; i++) {
              if (bestparams(i) + deltaparams(i) < 0) {
                // Can't do Newton step. Revert to descent.
                reverttograd = true;
              }
            }
          }
          if (reverttograd) {
            cerr << "Reverting to gradient, since Newton step infeasible:"
                 << endl;
          }
        }

        if (reverttograd) {
          stepsize = maxbestgradstep;
          deltaparams = -bestgrad * stepsize;
        }

        params = bestparams + deltaparams;
        cerr << "Change in weights from best, step size " << stepsize << ": "
             << endl;
        cerr << deltaparams << endl;
      } else {
        // Last attempt failed at being better, so move in gradient direction
        // with reduced step.
        // stepsize reduction factor is empirical
        stepsize = std::min(stepdecreasefactor * stepsize, maxbestgradstep);
        cerr << "Taking smaller step: " << stepsize << endl;
        params = bestparams - bestgrad * stepsize;
      }
      // Clean the parameters up.
      double sumparams = 0.0;
      for (std::size_t i = 0; i < nlambdas; i++) {
        if (!AllowNegativeParams) {
          if (params(i) < 1e-12) {
            // snap to zero, for active set and duplicate weights
            params(i) = 0;
          }
        }
        sumparams += params(i);
      }
      if (!AllowExtrapolation) params /= sumparams;

      bool duplicateentry = false;
      for (std::size_t i = 0; i < paramhistory.size(); ++i) {
        if (params == paramhistory[i]) duplicateentry = true;
      }
      while ((duplicateentry) && (stepsize >= minstepsize)) {
        cerr << "Duplicate weight found: " << endl;
        cerr << params << endl;
        stepsize *= 0.5;  // Step in this direction is duplicate, so try again
                          // with smaller step
        params = bestparams - stepsize * bestgrad;

        sumparams = 0.0;
        for (std::size_t i = 0; i < nlambdas; i++) {
          if (!AllowNegativeParams) {
            if (params(i) < 1e-12) params(i) = 0;
          }
          sumparams += params(i);
        }
        if (!AllowExtrapolation) params /= sumparams;

        duplicateentry = false;
        for (std::size_t i = 0; i < paramhistory.size(); ++i) {
          if (params == paramhistory[i]) duplicateentry = true;
        }
      }
      if (stepsize < minstepsize) break;  // No need to make another step
    }

    params = bestparams;  // So that next setiter is correct
    cerr << "Training complete. Best weights:" << endl;
    cerr << params << endl;
  }
}

int main(int argc, char **argv) {
  std::string tuning_data;
  std::vector<std::string> lms;

  try {
    namespace po = boost::program_options;
    po::options_description options("train-params");

    options.add_options()("help,h", po::bool_switch(),
                          "Show this help message")(
        "no_bias_term,B", po::bool_switch(), "Do not include a 'bias' feature")(
        "tuning_data,t", po::value<std::string>(&tuning_data),
        "File to tune perplexity on")(
        "model,m", po::value<std::vector<std::string> >(&lms),
        "Language models in KenLM format to interpolate");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, options), vm);

    // Display help
    if (argc == 1 || vm["help"].as<bool>()) {
      std::cerr << options << std::endl;
      return 1;
    }
    if (vm["no_bias_term"].as<bool>()) HAS_BIAS = false;
    lms = vm["model"].as<std::vector<std::string> >();
    tuning_data = vm["tuning_data"].as<std::string>();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }
  if (lms.size() < 2) {
    std::cerr
        << "Please specify at least two language model files with -m LM.KLM\n";
    return 1;
  }
  if (tuning_data.empty()) {
    std::cerr << "Please specify tuning set with -t FILE.TXT\n";
    return 1;
  }

  // Growable vocab here
  // GrowableVocab gvoc(100000); //dummy default

  // no comment
  std::map<std::string, int *> vmap;

  // stuff it into the
  EnumerateGlobalVocab *globalVocabBuilder =
      new EnumerateGlobalVocab(&vmap, lms.size());

  Config cfg;
  cfg.enumerate_vocab = (EnumerateVocab *)globalVocabBuilder;

  // load models
  std::vector<Model *> models;
  for (std::size_t i = 0; i < lms.size(); i++) {
    std::cerr << "Loading LM file: " << lms[i] << std::endl;

    // haaaack
    globalVocabBuilder->SetCurModel(i);  // yes this is dumb

    Model *this_model = new Model(lms[i].c_str(), cfg);
    models.push_back(this_model);
  }

  // assemble vocabulary vector
  std::vector<std::string> vocab;
  std::cerr << "Global Vocab Map has size: " << vmap.size() << std::endl;

  for (std::map<std::string, int *>::iterator iter = vmap.begin();
       iter != vmap.end(); ++iter) {
    vocab.push_back(iter->first);
  }
  std::cerr << "Vocab vector has size: " << vocab.size() << std::endl;

  // load context sorted ngrams into vector of vectors
  std::vector<std::vector<std::string> > corpus;

  std::cerr << "Loading context-sorted ngrams: " << tuning_data << std::endl;
  std::ifstream infile(tuning_data.c_str());

  for (std::string line; std::getline(infile, line);) {
    std::vector<std::string> words;
    std::stringstream stream(line);
    std::string word;

    while (stream >> word)
      words.push_back(word);
    corpus.push_back(words);
  }

  train_params(corpus, vocab, models);

  return 0;
}
