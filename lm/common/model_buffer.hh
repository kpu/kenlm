#ifndef LM_BUILDER_MODEL_BUFFER_H
#define LM_BUILDER_MODEL_BUFFER_H

/* Format with separate files in suffix order.  Each file contains
 * n-grams of the same order.
 */

#include "util/file.hh"
#include "util/fixed_array.hh"

#include <string>

namespace util { namespace stream { class Chains; } }

namespace lm { namespace common {

class ModelBuffer {
  public:
    // Construct for writing.
    ModelBuffer(const std::string &file_base, bool keep_buffer, bool output_q);

    // Load from file.
    explicit ModelBuffer(const std::string &file_base);

    // explicit for virtual destructor.
    ~ModelBuffer();

    void Sink(util::stream::Chains &chains);

    void Source(util::stream::Chains &chains);

    // The order of the n-gram model that is associated with the model buffer.
    std::size_t Order() const;

  private:
    const std::string file_base_;
    const bool keep_buffer_;
    bool output_q_;

    util::FixedArray<util::scoped_fd> files_;
};

}} // namespaces

#endif // LM_BUILDER_MODEL_BUFFER_H
