#ifndef BOUNDED_INPUT__
#define BOUNDED_INPUT__

// TODO: shrink Chunk to size on finish.  

#include "bounded/client.hh"

#include <cstddef>

namespace bounded {

class Chunk;

class Input {
  public:
    virtual ~Input();
    
    std::size_t Take() = 0;

  protected:
    explicit Input(Chunk &chunk);

    Chunk &chunk_;
};

// Provides unbounded zeroed memory.  
class MMapInput : public Input {
  public:
    MMapInput(Manager &manager, Chunk &chunk);

    std::size_t Take();

  private:
    Manager &manager_;
};

class ReadAhead : public Input {
  public:
    ReadAhead(const Config &config, Chunk &chunk, Input *inner);

    ~ReadAhead();

    std::size_t Take();

  private:
    void Reader();

    boost::scoped_ptr<Input> inner_;

    std::size_t available_;
    bool finished_;
    // Fires when available_ or finished_ changed.  
    boost::condition_variable update_cond_;
    // Mutex covering available_ and finished_.
    boost::mutex update_mutex_;

    const std::size_t read_ahead_size_;

    boost::thread ahead_;
};

// Currently in RAM (as chunk) but we might be called upon to spill it.  
class MemoryInput : public Input, public SpillClient {
  public:
    MemoryInput(Manager &manager, Chunk &chunk);

    ~MemoryInput();

    std::size_t Take();

    std::size_t SpillableSize();

    void Spill();

  private:
    // Amount left in chunk_ (not in disk).  
    std::size_t spillable_;
    boost::mutex spillable_mutex_;

    // Parts on disk.  
    util::scoped_fd file_;

    struct Record {
      std::off_t off;
      std::size_t size;
    };

    std::size_t next_start_;

    // Records spilled to file_.  The earliest part of the memory is on top of the stack and at the end of the file.  
    std::stack<Record> dumps_;

    boost::mutex file_mutex_;
};

} // namespace bounded

#endif // BOUNDED_INPUT__
