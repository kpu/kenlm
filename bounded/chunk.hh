#ifndef BOUNDED_CHUNK__
#define BOUNDED_CHUNK__

#include <boost/noncopyable.hpp>
#include <boost/thread/shared_mutex.hpp>

#include <cstddef>

namespace bounded {

// Chunk of memory.  This does not notify the Manager in any way.  Callers are responsible for locking.  
class Chunk : boost::noncopyable {
  public:
    Chunk(const Config &config, std::size_t element_size);

    ~Chunk();

    // A read lock means pointers remain valid (i.e. GrowEnd can't run).  
    boost::shared_mutex &MappingMutex() { return mapping_; }

    /* Reference point.  Subtract from pointers, unlock MappingMutex(), do
     * something, lock MappingMutex(), add to offsets.  This address may not be
     * mapped.  In fact, it may be greater than Begin() if mmap gave a small
     * enough address.
     */
    void *Base() const { return base_; }

    void *BasePlus(std::size_t amount) {
      return reinterpret_cast<uint8_t*>(base_) + amount;
    }

    std::size_t MinusBase(void *pointer) const {
      return reinterpret_cast<uint8_t*>(pointer) - reinterpret_cast<uint8_t*>(base_);
    }

    // Do not call ShrinkBegin or GrowEnd conccurrently.  
    void *Begin() { return begin_; }

    // Do not call ShrinkEnd or GrowEnd conccurently.  
    void *End() { return end_; }

    // Free memory at the beginning.  
    // Doesn't change Base.  
    // Do not call ShrinkBegin (twice), Begin, or GrowEnd conccurently.  
    void ShrinkBegin(std::size_t amount);

    // Free memory at the end.  
    // Doesn't change Base.
    // Do not call ShrinkEnd (twice), End, or GrowEnd concurrently.  
    void ShrinkEnd(std::size_t amount);

    // Unlike the other calls, this does its own locking, acquiring a unique lock on MappingMutex().  
    // Do not call Base, Begin, End, ShrinkBegin, ShrinkEnd, or GrowEnd (twice) concurrently.  
    void GrowEnd(std::size_t amount);

    std::size_t ElementSize() const { return element_size_; }
    std::size_t BlockSize() const { return block_size_; }

    void Swap(Chunk &other);

  private:
    void *base_;

    // This controls changes to the overall mapping address.  
    boost::shared_mutex mapping_mutex_;

    void *begin_;
    // End visible to users.  
    void *end_;
    // end_ rounded up to next page.  
    void *actual_end_;

    const std::size_t element_size_, block_size_;
};

} // namespace bounded

#endif // BOUNDED_CHUNK__
