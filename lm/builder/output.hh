#ifndef LM_BUILDER_OUTPUT_H
#define LM_BUILDER_OUTPUT_H

#include "lm/builder/header_info.hh"
#include "util/file.hh"

#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/utility.hpp>

namespace util { namespace stream { class Chains; class ChainPositions; } }

/* Outputs from lmplz: ARPA, sharded files, etc */
namespace lm { namespace builder {

// These are different types of hooks.  Values should be consecutive to enable a vector lookup.
enum HookType {
  // TODO: counts.
  PROB_PARALLEL_HOOK, // Probability and backoff (or just q).  Output must process the orders in parallel or there will be a deadlock.
  PROB_SEQUENTIAL_HOOK, // Probability and backoff (or just q).  Output can process orders any way it likes.  This requires writing the data to disk then reading.  Useful for ARPA files, which put unigrams first etc.
  NUMBER_OF_HOOKS // Keep this last so we know how many values there are.
};

class Output;

class OutputHook {
  public:
    explicit OutputHook(HookType hook_type) : type_(hook_type), master_(NULL) {}

    virtual ~OutputHook();

    virtual void Sink(util::stream::Chains &chains) = 0;

  protected:
    const HeaderInfo &GetHeader() const;
    int GetVocabFD() const;

  private:
    friend class Output;
    const HookType type_;
    const Output *master_;
};

class Output : boost::noncopyable {
  public:
    Output(StringPiece file_base, bool keep_buffer);

    // Takes ownership.
    void Add(OutputHook *hook) {
      hook->master_ = this;
      outputs_[hook->type_].push_back(hook);
    }

    bool Have(HookType hook_type) const {
      return !outputs_[hook_type].empty();
    }

    void SetVocabFD(int to) { vocab_fd_ = to; }
    int GetVocabFD() const { return vocab_fd_; }

    void SetHeader(const HeaderInfo &header) { header_ = header; }
    const HeaderInfo &GetHeader() const { return header_; }

    // This is called by the pipeline.
    void SinkProbs(util::stream::Chains &chains, bool output_q);

    unsigned int Steps() const { return Have(PROB_SEQUENTIAL_HOOK); }

  private:
    void Apply(HookType hook_type, util::stream::Chains &chains);

    boost::ptr_vector<OutputHook> outputs_[NUMBER_OF_HOOKS];
    int vocab_fd_;
    HeaderInfo header_;

    std::string file_base_;
    bool keep_buffer_;
};

inline const HeaderInfo &OutputHook::GetHeader() const {
  return master_->GetHeader();
}

inline int OutputHook::GetVocabFD() const {
  return master_->GetVocabFD();
}

}} // namespaces

#endif // LM_BUILDER_OUTPUT_H
