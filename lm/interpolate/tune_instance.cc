#include "lm/interpolate/tune_instance.hh"

#include "lm/common/model_buffer.hh"
#include "lm/common/ngram_stream.hh"
#include "lm/common/renumber.hh"
#include "lm/enumerate_vocab.hh"
#include "lm/interpolate/merge_vocab.hh"
#include "lm/interpolate/universal_vocab.hh"
#include "lm/lm_exception.hh"
#include "util/file_piece.hh"
#include "util/murmur_hash.hh"
#include "util/stream/chain.hh"
#include "util/tokenize_piece.hh"

#include <boost/unordered_map.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace lm { namespace interpolate {

// An extension without backoff weights applied yet.
#pragma pack(push)
#pragma pack(1)
struct InitialExtension {
  Extension ext;
  // Order from which it came.
  uint8_t order;
};
#pragma pack(pop)

// Intended use
// For each model:
//   stream through orders jointly in suffix order:
//     Call MatchedBackoff for full matches.
//     Call Exit when the context matches.
//   Call FinishModel with the unigram probability of the correct word, get full
//   probability in return.
// Use Backoffs to adjust records that were written to the stream.
class InstanceMatch {
  public:
    InstanceMatch(ModelIndex models, uint8_t max_order, const WordIndex correct)
      : seen_(std::numeric_limits<WordIndex>::max()),
        backoffs_(Matrix::Zeros(models, max_order)),
        correct_(correct), correct_from_(1), correct_ln_prob_(std::numeric_limits<float>::quiet_NaN()) {}

    void MatchedBackoff(ModelIndex model, uint8_t order, float ln_backoff) {
      backoffs_(model, order - 1) = ln_backoff;
    }

    // We only want the highest-order matches, which are the first to be exited for a given word.
    void Exit(const InitialExtension &from, util::stream::Stream &out) {
      if (from.ext.word == seen_) return;
      seen_ = from.ext.word;
      *static_cast<InitialExtension*>(out.Get()) = from;
      ++out;
      if (UTIL_UNLIKELY(correct_ == from.ext.word)) {
        correct_from_ = from.order;
        correct_ln_prob_ = from.ext.ln_prob;
      }
    }

    WordIndex Correct() const { return correct_; }

    // Call this after each model has been passed through.  The 
    float FinishModel(ModelIndex model, float correct_ln_unigram) {
      seen_ = std::numeric_limits<WordIndex>::max();
      // Turn backoffs into multiplied values (added in log space).
      // So backoffs_(model, order - 1) is the penalty for matching order.
      float accum = 0.0;
      for (int order = backoffs_.cols() - 1; order >= 0; --order) {
        accum += backoffs_(model, order);
        backoffs_(model, order) = accum;
      }
      if (correct_from_ == 1) {
        correct_ln_prob_ = correct_ln_unigram;
      }
      if (correct_from_ - 1 < backoffs_.cols()) {
        correct_ln_prob_ += backoffs_(model, correct_from_ - 1);
      }
      correct_from_ = 1;
      return correct_ln_prob_;
    }

    const Matrix &Backoffs() const {
      return backoffs_;
    }

  private:
    // What's the last word we've seen?  Used to act only on exiting the longest match.
    WordIndex seen_;

    Matrix backoffs_;

    const WordIndex correct_;

    // These only apply to the most recent model.
    uint8_t correct_from_;

    float correct_ln_prob_;
};

namespace {

// Forward information to multiple instances of a context.
class DispatchContext {
  public:
    void Register(InstanceMatch &context) {
      registered_.push_back(&context);
    }

    void MatchedBackoff(uint8_t order, float ln_backoff) {
      for (std::vector<InstanceMatch*>::iterator i = registered_.begin(); i != registered_.end(); ++i)
        (*i)->MatchedBackoff(order, ln_backoff);
    }

    void Exit(const InitialExtension &from, util::stream::Stream &out) {
      for (std::vector<InstanceMatch*>::iterator i = registered_.begin(); i != registered_.end(); ++i) {
        (*i)->Exit(from, out);
      }
    }

  private:
    std::vector<InstanceMatch*> registered_;
};

// Map from n-gram hash to contexts in the tuning data.
typedef boost::unordered_map<uint64_t, DispatchContext> ContextMap;

class ApplyBackoffs {
  public:
    explicit ApplyBackoffs(const InstanceMatch *backoffs) : backoffs_(backoffs) {}

    void Run(const util::stream::ChainPosition &position) {
      for (util::stream::Stream stream(position); stream; ++stream) {
        InitialExtension &ini = *reinterpret_cast<InitialExtension*>(stream.Get());
        ini.ext.ln_prob += backoffs_[ini.ext.instance] 
      }
    }

  private:
    const InstanceMatch *backoffs_;
};

Instances::ReadExtensions(util::stream::Chain &on) {
  if (extensions_first_.get()) {
    // Lazy sort and save a sorted copy to disk.  TODO: cut down on record size by stripping out order information?
    extensions_first_->Output(on);
    extensions_first_->reset();
    // TODO: apply backoff data!!!!

    extensions_subsequent_.reset(new util::stream::FileBuffer(util::MakeTemp(sorting_config_.temp_prefix)));
    on >> extensions_subsequent_->Sink();
  } else {
    on >> extensions_subsequent_->Source();
  }
}

class UnigramLoader {
  public:
    UnigramLoader(ContextMap &contexts_for_backoffs, Matrix &ln_probs, std::size_t model_number)
      : map_(contexts_for_backoffs),
        prob_(ln_probs.col(model_number)) {}

    void Run(const util::stream::ChainPosition &position) {
      // TODO handle the case of a unigram model?
      NGramStream<ProbBackoff> input(position);
      assert(input);
      Accum unk = input->Value().prob * M_LN10;
      WordIndex previous = 0;
      for (; input; ++input) {
        WordIndex word = *input->begin();
        prob_.segment(previous, word - previous) = Vector::Constant(word - previous, unk);
        prob_(word) = input->Value().prob * M_LN10;
        ContextMap::iterator i = map_.find(util::MurmurHashNative(input->begin(), sizeof(WordIndex)));
        if (i != map_.end()) {
          i->second.MatchedBackoff(1, input->Value().backoff * M_LN10);
        }
        previous = word + 1;
      }
      prob_.segment(previous, prob_.rows() - previous) = Vector::Constant(prob_.rows() - previous, unk);
    }

  private:
    ContextMap &map_;
    Matrix::ColXpr prob_;
    std::size_t model_;
};

class MiddleLoader {
  public:
    explicit MiddleLoader(ContextMap &map)
      : map_(map) {}

    void Run(const util::stream::ChainPosition &position) {
      NGramStream<ProbBackoff> input(position);
      const std::size_t full_size = (uint8_t*)input->end() - (uint8_t*)input->begin();
      const std::size_t context_size = full_size - sizeof(WordIndex);
      ContextMap::iterator i;
      for (; input; ++input) {
        i = map_.find(util::MurmurHashNative(input->begin(), full_size));
        if (i != map_.end()) {
          i->second.MatchedBackoff(input->Order(), input->Value().backoff * M_LN10);
        }
        i = map_.find(util::MurmurHashNative(input->begin(), context_size));
        if (i != map_.end()) {
          i->second.MatchedContext(input->Order(), *(input->end() - 1), input->Value().prob * M_LN10);
        }
      }
    }

  private:
    ContextMap &map_;
};

class HighestLoader {
  public:
    HighestLoader(ContextMap &map, uint8_t order)
      : map_(map), order_(order) {}

    void Run(const util::stream::ChainPosition &position) {
      ContextMap::iterator i;
      const std::size_t context_size = sizeof(WordIndex) * (order_ - 1);
      for (ProxyStream<NGram<float> > input(position, NGram<float>(NULL, order_)); input; ++input) {
        i = map_.find(util::MurmurHashNative(input->begin(), context_size));
        if (i != map_.end()) {
          i->second.MatchedContext(order_, *(input->end() - 1), input->Value() * M_LN10);
        }
      }
    }

  private:
    ContextMap &map_;
    const uint8_t order_;
};

class IdentifyTuning : public EnumerateVocab {
  public:
    IdentifyTuning(int tuning_file, std::vector<WordIndex> &out) : indices_(out) {
      indices_.clear();
      StringPiece line;
      std::size_t counter = 0;
      std::vector<std::size_t> &eos = words_[util::MurmurHashNative("</s>", 4)];
      for (util::FilePiece f(tuning_file); f.ReadLineOrEOF(line);) {
        for (util::TokenIter<util::BoolCharacter, true> word(line, util::kSpaces); word; ++word) {
          UTIL_THROW_IF(*word == "<s>" || *word == "</s>", FormatLoadException, "Illegal word in tuning data: " << *word);
          words_[util::MurmurHashNative(word->data(), word->size())].push_back(counter++);
        }
        eos.push_back(counter++);
      }
      // Also get <s>
      indices_.resize(counter + 1);
      words_[util::MurmurHashNative("<s>", 3)].push_back(indices_.size() - 1);
    }

    void Add(WordIndex id, const StringPiece &str) {
      boost::unordered_map<uint64_t, std::vector<std::size_t> >::iterator i = words_.find(util::MurmurHashNative(str.data(), str.size()));
      if (i != words_.end()) {
        for (std::vector<std::size_t>::iterator j = i->second.begin(); j != i->second.end(); ++j) {
          indices_[*j] = id;
        }
      }
    }

    WordIndex FinishGetBOS() {
      WordIndex ret = indices_.back();
      indices_.pop_back();
      return ret;
    }

  private:
    std::vector<WordIndex> &indices_;

    boost::unordered_map<uint64_t, std::vector<std::size_t> > words_;
};

} // namespace

Instance::Instance(std::size_t num_models) : ln_backoff(num_models), ln_correct(num_models), ln_extensions(0, num_models) {}

WordIndex LoadInstances(int tuning_file, const std::vector<StringPiece> &model_names, util::FixedArray<Instance> &instances, Matrix &ln_unigrams) {
  util::FixedArray<ModelBuffer> models(model_names.size());
  std::vector<WordIndex> vocab_sizes;
  vocab_sizes.reserve(model_names.size());
  util::FixedArray<util::scoped_fd> vocab_files(model_names.size());
  std::size_t max_order = 0;
  for (std::vector<StringPiece>::const_iterator i = model_names.begin(); i != model_names.end(); ++i) {
    models.push_back(*i);
    vocab_sizes.push_back(models.back().Counts()[0]);
    vocab_files.push_back(models.back().StealVocabFile());
    max_order = std::max(max_order, models.back().Order());
  }
  UniversalVocab vocab(vocab_sizes);
  std::vector<WordIndex> tuning_words;
  WordIndex bos;
  WordIndex combined_vocab_size;
  {
    IdentifyTuning identify(tuning_file, tuning_words);
    combined_vocab_size = MergeVocab(vocab_files, vocab, identify);
    bos = identify.FinishGetBOS();
  }

  instances.Init(tuning_words.size());
  util::FixedArray<InstanceBuilder> builders(tuning_words.size());
  std::vector<WordIndex> context;
  context.push_back(bos);

  // Populate the map from contexts to instance builders.
  ContextMap cmap;
  const WordIndex eos = tuning_words.back();
  for (std::size_t i = 0; i < tuning_words.size(); ++i) {
    instances.push_back(model_names.size());
    builders.push_back(tuning_words[i], max_order);
    for (std::size_t j = 0; j < context.size(); ++j) {
      cmap[util::MurmurHashNative(&context[j], sizeof(WordIndex) * (context.size() - j))].Register(builders.back());
    }
    // Prepare for next word.
    if (tuning_words[i] == eos) {
      context.clear();
      context.push_back(bos);
    } else {
      if (context.size() == max_order) {
        context.erase(context.begin());
      }
      context.push_back(tuning_words[i]);
    }
  }

  ln_unigrams.resize(combined_vocab_size, models.size());

  // Scan through input files.  Sadly not parallel due to an underlying hash table.
  for (std::size_t m = 0; m < models.size(); ++m) {
    for (std::size_t order = 1; order <= models[m].Order(); ++order) {
      util::stream::Chain chain(util::stream::ChainConfig(sizeof(ProbBackoff) + order * sizeof(WordIndex), 2, 64 * 1048576));
      models[m].Source(order - 1, chain);
      chain >> Renumber(vocab.Mapping(m), order);
      if (order == 1) {
        chain >> UnigramLoader(cmap, ln_unigrams, m);
      } else if (order < models[m].Order()) {
        chain >> MiddleLoader(cmap);
      } else {
        chain >> HighestLoader(cmap, order);
      }
    }
    for (std::size_t instance = 0; instance < tuning_words.size(); ++instance) {
      builders[instance].Dump(m, ln_unigrams, instances[instance]);
    }
    ln_unigrams(bos, m) = -99; // Does not matter as long as it does not produce nans since tune_derivatives sets this to zero.
  }
  return bos;
}

}} // namespaces
