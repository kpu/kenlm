#include "lm/builder/output.hh"
#include "util/stream/multi_stream.hh"

#include <boost/ref.hpp>

namespace lm { namespace builder {

OutputHook::~OutputHook() {}

void OutputHook::Apply(util::stream::Chains &chains) {
  chains >> boost::ref(*this);
}

}} // namespaces
