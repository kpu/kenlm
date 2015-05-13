#include <string>
#include <vector>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/version.hpp>

#include "util/fixed_array.hh"

#include <Eigen/Eigen>

typedef Eigen::MatrixXf FMatrix;
typedef Eigen::VectorXf FVector;

bool HAS_BIAS = true;

inline float logProb(unsigned model, const std::vector<std::string>& ctx, const std::string& word) {
  // TODO
  return 0;
}

void set_features(const std::vector<std::string>& ctx,
                  const std::string& word,
                  const std::vector<unsigned>& models,
                  FVector& v) {
  if (HAS_BIAS) {
    v(0) = 1;
    for (unsigned i=0; i < models.size(); ++i)
      v(i + 1) = logProb(models[i], ctx, word);
  } else {
    for (unsigned i=0; i < models.size(); ++i)
      v(i) = logProb(models[i], ctx, word);
  }
}

void train_params(
    const std::vector<std::vector<std::string> >& corpus,
    const std::vector<std::string>& vocab,
    const std::vector<unsigned>& models) {
  using namespace std;
  vector<string> context(5, "<s>");
  const int ITERATIONS = 10;
  const int nlambdas = models.size() + HAS_BIAS ? 1 : 0; // bias + #models
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
      context.resize(5);
      for (unsigned t = 0; t < sentence.size(); ++t) { // words in sentence
        ++numchars;
        const string& ref_word_string = sentence[t];
        int ref_word = 0; // TODO
        double z = 0;
        for (unsigned i = 0; i < vocab.size(); ++i) { // vocab
          set_features(context, vocab[i], models, feats[i]);
          us[i] = params.dot(feats[i]);
          z += exp(double(us[i]));
        }
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
    LM_FILES = vm["model"].as<std::vector<std::string> >();
    TUNING_DATA = vm["tuning_data"].as<std::string>();
  }
  catch(const std::exception &e) {

    std::cerr << e.what() << std::endl;
    return 1;

  }
  if (LM_FILES.size() < 2) {
    std::cerr << "Please specify at least two language model files with -m LM.KLM\n";
    return 1;
  }
  if (TUNING_DATA.empty()) {
    std::cerr << "Please specify tuning set with -t FILE.TXT\n";
    return 1;
  }
  return 0;
}



