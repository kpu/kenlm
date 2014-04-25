#include "lm/builder/interpolate.hh"

#include "lm/blank.hh"
#include "lm/builder/binarize.hh"
#include "lm/builder/joint_order.hh"
#include "lm/builder/multi_stream.hh"
#include "lm/builder/sort.hh"
#include "lm/lm_exception.hh"

#include <iostream>
#include <numeric>

#include <assert.h>

namespace lm { namespace builder {
namespace {

class Callback {
  public:
    Callback(float uniform_prob, Binarize &binarize, const ChainPositions &backoffs)
      : full_backoffs_(backoffs.size()), q_files_(backoffs.size() + 1), q_chain_(backoffs.size() + 1), q_out_(backoffs.size() + 1), probs_(backoffs.size() + 2 /* zeroton has uniform prob */), q_delta_(backoffs.size() + 1), binarize_(binarize) {
      // This stays forever as the zeroton.
      probs_[0] = uniform_prob;
      for (std::size_t i = 0; i < backoffs.size(); ++i) {
        full_backoffs_.push_back(backoffs[i]);
      }
      util::stream::ChainConfig write_qs;
      write_qs.total_memory = 1048576;
      write_qs.block_count = 2;
      for (std::size_t i = 0; i < backoffs.size() + 1; ++i) {
        write_qs.entry_size = (i + 1) * sizeof(WordIndex) + 4;
        std::string file("q");
        file += boost::lexical_cast<std::string>(i + 1);
        q_files_.push_back(util::CreateOrThrow(file.c_str()));
        q_chain_.push_back(write_qs);
        q_out_.push_back(q_chain_.back().Add());
        q_chain_.back() >> util::stream::WriteAndRecycle(q_files_.back().get());
      }
    }

    ~Callback() {
      for (std::size_t i = 0; i < q_chain_.size(); ++i) {
        q_out_[i].Poison();
      }
      for (std::size_t i = 0; i < full_backoffs_.size(); ++i) {
        if (full_backoffs_[i]) {
          std::cerr << "Backoffs do not match for order " << (i + 1) << std::endl;
          //abort();
        }
      }
    }

    void Enter(unsigned order_minus_1, NGram &gram) {
      Payload &pay = gram.Value();
      // q delta subtracts the context's backoff
      if (order_minus_1) {
        q_delta_[order_minus_1] = q_delta_[order_minus_1 - 1] / pay.uninterp.gamma;
      } else {
        q_delta_[0] = 1.0;
      }

      pay.complete.prob = pay.uninterp.prob + pay.uninterp.gamma * probs_[order_minus_1];
      float actual_prob = pay.complete.prob;
      probs_[order_minus_1 + 1] = pay.complete.prob;
      pay.complete.prob = log10(pay.complete.prob);
      // TODO: this is a hack to skip n-grams that don't appear as context.  Pruning will require some different handling.  
      if (order_minus_1 < full_backoffs_.size() && *(gram.end() - 1) != kUNK && *(gram.end() - 1) != binarize_.EndSentence()) {
        float full_backoff = *static_cast<const float*>(full_backoffs_[order_minus_1].Get());
        q_delta_[order_minus_1] *= full_backoff;
        pay.complete.backoff = log10(full_backoff);
        ngram::SetExtension(pay.complete.backoff);
        ++full_backoffs_[order_minus_1];
      } else {
        // Not a context.  
        pay.complete.backoff = ngram::kNoExtensionBackoff;
      }
      // Write the q value to a stream with n-grams.
      std::size_t gram_size = sizeof(WordIndex) * gram.Order();
      memcpy(q_out_[order_minus_1].Get(), gram.begin(), gram_size);
      *reinterpret_cast<float*>(static_cast<uint8_t*>(q_out_[order_minus_1].Get()) + gram_size) = q_delta_[order_minus_1] * actual_prob;
      ++q_out_[order_minus_1];
      binarize_.Enter(order_minus_1, gram);
    }

   // void Exit(unsigned, const NGram &) const {}

  private:
    FixedArray<util::stream::Stream> full_backoffs_;

    FixedArray<util::scoped_fd> q_files_;

    FixedArray<util::stream::Chain> q_chain_;

    FixedArray<util::stream::Stream> q_out_;

    std::vector<float> probs_, q_delta_;

    Binarize &binarize_;
};
} // namespace

Interpolate::Interpolate(uint64_t vocab_size, Binarize &binarize, const ChainPositions &backoffs)
  : uniform_prob_(1.0 / static_cast<float>(vocab_size)), // Includes <unk> but excludes <s>.
    binarize_(binarize),
    backoffs_(backoffs) {}

// perform order-wise interpolation
void Interpolate::Run(const ChainPositions &positions) {
  assert(positions.size() == backoffs_.size() + 1);
  Callback callback(uniform_prob_, binarize_, backoffs_);
  JointOrder<Callback, SuffixOrder>(positions, callback);
}

}} // namespaces
