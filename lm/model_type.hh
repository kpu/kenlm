#ifndef LM_MODEL_TYPE__
#define LM_MODEL_TYPE__

namespace lm {
namespace ngram {

/* Not the best numbering system, but it grew this way for historical reasons
 * and I want to preserve existing binary files. */
typedef enum {HASH_PROBING=0, HASH_SORTED=1, TRIE_SORTED=2, QUANT_TRIE_SORTED=3, ARRAY_TRIE_SORTED=4, QUANT_ARRAY_TRIE_SORTED=5} ModelType;

const static ModelType kQuantAdd = static_cast<ModelType>(QUANT_TRIE_SORTED - TRIE_SORTED);
const static ModelType kArrayAdd = static_cast<ModelType>(ARRAY_TRIE_SORTED - TRIE_SORTED);

} // namespace ngram
} // namespace lm
#endif // LM_MODEL_TYPE__
