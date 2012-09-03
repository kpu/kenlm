#ifndef BOUNDED_OUTPUT__
#define BOUNDED_OUTPUT__

#include "bounded/client.hh"

#include <boost/thread/mutex.hpp>

#include <inttypes.h>

namespace bounded {

class Source;

class Output {
  public:
    virtual ~Output();

    virtual void Give(std::size_t amount) = 0;

  protected:
    explicit Output(Chunk &chunk);

    Chunk &chunk_;
};

class DiscardOutput : public Output {
  public:
    DiscardOutput(Manager &manager, Chunk &chunk);

    void Give(std::size_t amount);

  private:
    Manager &manager_;
};

class KeepOutput : public SpillClient, public Output {
  public:
    virtual ~KeepOutput();

    std::size_t SpillableSize();

    void Spill();

    // Increase ownership range to here.  
    void Give(std::size_t amount);

    Source *Finish();

  protected:
    KeepOutput(Manager &manager, Chunk &chunk);

    // Dump [chunk_.Begin(), chunk_.Begin() + amount).  It's the callee's responsibility to call chunk_.ShrinkBegin and report to the manager.  The reason for this is to support incremental updates.  
    virtual void DumpAndShrink(std::size_t amount) = 0;

    virtual Source *FinishBackend(std::size_t remaining) = 0;

  private:
    boost::mutex spilling_;

    // Size of memory owned by this.  
    std::size_t own_;

    boost::mutex own_mutex_;
};

} // namespace bounded

#endif // BOUNDED_OUTPUT__
