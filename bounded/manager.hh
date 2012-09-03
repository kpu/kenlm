#ifndef BOUNDED_MANAGER__
#define BOUNDED_MANAGER__

#include "bounded/config.hh"

#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>

#include <set>
#include <vector>

namespace bounded {

class Client;
class SpillClient;

class Manager {
  public:
    explicit Manager(const Config &config);

    ~Manager();

    void AddSpiller(SpillClient &client);
    void RemoveSpiller(SpillClient &client);

    void Grow(std::size_t amount);
    void Shrink(std::size_t amount);

    const Config &GetConfig() const { return config_; }

    // Lock this to prevent the spiller from running.  May cause memory requests to wait until the lock is released.  
    boost::shared_mutex &SpillMutex() { return spiller_mutex_; }

  private:
    // Spiller thread. 
    void Spiller();

    std::vector<SpillClient*> spillable_;
    boost::mutex spillable_mutex_;

    // Amount of memory left to hand out.  
    std::size_t remaining_;

    // Total of pending request for bytes.  
    std::size_t request_total_;
    // Covers both remaining_ and request_total_.  
    boost::mutex size_mutex_;

    // Memory below which a spill is preemptively triggered.  
    const std::size_t threshold_;

    // Fires when space is freed.  
    boost::condition_variable space_;
    // Fires when space is allocated.  
    boost::condition_variable spill_;

    const Config config_;

    boost::thread spiller_;

    // Locked when spiller is running.  
    boost::shared_mutex spiller_mutex_;
};

} // namespace bounded

#endif // BOUNDED_MANAGER__
