#include "bounded/output.hh"

#include <boost/thread/shared_mutex.hpp>
#include <boost/math/common_factor_rt.hpp>

namespace bounded {

Output::~Output() {}

Output::Output(Chunk &chunk) : chunk_(chunk) {}

DiscardOutput::DiscardOutput(Manager &manager, Chunk &chunk) : Output(chunk), manager_(manager) {}

void DiscardOutput::Give(std::size_t amount) {
  {
    boost::shared_lock<boost::shared_mutex> lock(chunk_.MappingMutex());
    chunk_.ShrinkBegin(amount);
  }
  manager_.Shrink(amount);
}

KeepOutput::~KeepOutput() {}

std::size_t KeepOutput::SpillableSize() {
  boost::lock_guard<boost::mutex> lock(own_mutex_);
  return own_;
}

void KeepOutput::Spill() {
  boost::lock_guard<boost::mutex> spill_lock(spilling_);
  std::size_t removing;
  {
    boost::lock_guard<boost::mutex> lock(own_mutex_);
    removing = own_;
    own_ = 0;
  }
  // Could happen for example when Finished is being called.  
  if (!removing) return;
  boost::shared_lock<boost::shared_mutex> lock(chunk_.MappingMutex());
  DumpAndShrink(removing);
}

void KeepOutput::Give(std::size_t amount) {
  boost::lock_guard<boost::mutex> lock(own_mutex_);
  own_ += amount;
  // This is manager-neutral because ownership was transferred to here.  
}

Source *KeepOutput::Finish() {
  // TODO: prevent Manager from dying if we're above the threshold.  
  manager_.RemoveSpiller(*this);
  {
    // If this thread wins the lock, the spiller will exit immediately.  If this thread loses the lock, the spiller will spill and there's northing left to sort here.  
    boost::lock_guard<boost::mutex> spill_lock(spilling_);
    std::size_t remaining;
    {
      boost::lock_guard<boost::mutex> lock(own_mutex_);
      remaining = own_;
      own_ = 0;
    }
  }
  // Spiller should not be running now.  
  boost::shared_lock<boost::shared_mutex> lock(chunk_.MappingMutex());
  return FinishBackend(remaining);
}

KeepOutput::KeepOutput(Manager &manager, Chunk &chunk) :
    SpillClient(manager), Output(chunk_),
    own_(0) {}


} // namespace bounded
