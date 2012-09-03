#include "bounded/manager.hh"

#include "bounded/client.hh"
#include "bounded/config.hh"

#include <algorithm>
#include <iostream>

#include <stdlib.h>

namespace bounded {

// TODO: configurable threshold.
Manager::Manager(const Config &config) : remaining_(config.total_memory), threshold_(config.threshold_memory), config_(config), spiller_(&Manager::Spiller, this) {}

Manager::~Manager() {
  spiller_.interrupt();
}

void Manager::AddSpiller(SpillClient &client) {
  boost::lock_guard<boost::mutex> lock(spillable_mutex_);
  spillable_.push_back(&client);
}

void Manager::RemoveSpiller(SpillClient &client) {
  boost::lock_guard<boost::mutex> lock(spillable_mutex_);
  // Could be more efficient
  std::vector<SpillClient*>::iterator i = std::find(spillable_.begin(), spillable_.end(), &client);
  if (i == spillable_.end()) return;
  *i = spillable_.back();
  spillable_.resize(spillable_.size() - 1);
}

void Manager::Grow(std::size_t amount) {
  boost::unique_lock<boost::mutex> lock(size_mutex_);
  while (remaining_ < amount) {
    request_total_ += amount;
    spill_.notify_one();
    space_.wait(lock);
  }
  remaining_ -= amount;
  request_total_ -= amount;
}

void Manager::Shrink(std::size_t amount) {
  {
    boost::lock_guard<boost::mutex> lock(size_mutex_);
    remaining_ += amount;
  }
  space_.notify_all();
}

struct LessBySize : public std::binary_function<const SpillClient *, const SpillClient *, bool> {
  bool operator()(const SpillClient *first, const SpillClient *second) const {
    return first->SpillableSize() < second->SpillableSize();
  }
};

// TODO: spill multiple things in parallel?  
void Manager::Spiller() {
  while (true) {
    boost::lock_guard<boost::shared_mutex> spiller_lock(SpillerMutex());
    {
      boost::unique_lock<boost::mutex> lock(size_mutex_);
      while (remaining_ >= threshold_ + request_total_) {
        spill_.wait(lock);
      }
    }
    SpillClient *victim;
    {
      boost::lock_guard<boost::mutex> lock(spillable_mutex_);

      std::vector<SpillClient*>::iterator victim_it = std::max_element(spillable_.begin(), spillable_.end());
      if (victim_it == spillable_.end()) {
        std::cerr << "Out of memory and nothing to spill." << std::endl;
        abort();
      }
      victim = *victim_it;
    }
    if (!victim->SpillableSize()) {
      std::cerr << "Out of memory and nothing left to spill." << std::endl;
      abort();
    }
    victim->Spill();
  }
}

} // namespace bounded
