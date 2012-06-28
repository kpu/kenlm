#ifndef LM_MAX_ORDER__
#define LM_MAX_ORDER__
namespace lm {
namespace ngram {
// If you need higher order, change this and recompile.  
// Having this limit means that State can be
// (kMaxOrder - 1) * sizeof(float) bytes instead of
// sizeof(float*) + (kMaxOrder - 1) * sizeof(float) + malloc overhead
const unsigned char kMaxOrder = 6;

} // namespace ngram
} // namespace lm

#endif // LM_MAX_ORDER__
