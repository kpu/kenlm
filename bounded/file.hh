#ifndef BOUNDED_FILE__
#define BOUNDED_FILE__

#include <cstddef>

namespace bounded {

class Config;
class Manager;
class Chunk;

int OpenTemp(const Config &config);

// Append [begin, end) to fd, releasing the memory. as we go.   
void AppendAndRelease(const Chunk &chunk, std::size_t amount, Manager &manager, int fd);

} // namespace bounded

#endif // BOUNDED_FILE__
