#include "lm/builder/output.hh"

#include "lm/common/model_buffer.hh"
#include "util/stream/multi_stream.hh"

#include <iostream>

namespace lm { namespace builder {

OutputHook::~OutputHook() {}

Output::Output(StringPiece file_base, bool keep_buffer)
  : file_base_(file_base.data(), file_base.size()), keep_buffer_(keep_buffer) {}

void Output::SinkProbs(util::stream::Chains &chains, bool output_q) {
  Apply(PROB_PARALLEL_HOOK, chains);
  if (!keep_buffer_ && !Have(PROB_SEQUENTIAL_HOOK)) {
    chains >> util::stream::kRecycle;
    chains.Wait(true);
    return;
  }
  lm::common::ModelBuffer buf(file_base_, keep_buffer_, output_q);
  buf.Sink(chains);
  chains >> util::stream::kRecycle;
  chains.Wait(false);
  if (Have(PROB_SEQUENTIAL_HOOK)) {
    std::cerr << "=== 5/5 Writing ARPA model ===" << std::endl;
    buf.Source(chains);
    Apply(PROB_SEQUENTIAL_HOOK, chains);
    chains >> util::stream::kRecycle;
    chains.Wait(true);
  }
}

void Output::Apply(HookType hook_type, util::stream::Chains &chains) {
  for (boost::ptr_vector<OutputHook>::iterator entry = outputs_[hook_type].begin(); entry != outputs_[hook_type].end(); ++entry) {
    entry->Sink(chains);
  }
}

}} // namespaces
