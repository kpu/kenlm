#ifndef LM_BUILDER_SUM_EXTEND__
#define LM_BUILDER_SUM_EXTEND__

namespace util { namespace stream { class ChainPosition; }}

namespace lm {
namespace builder {

/* Compute \sum_w a(w | c) for all c.  This is the denominator in Section 3 of
 * Chen and Goodman.  
 *
 * Input: context sorted n-grams with adjusted counts.
 * Output: suffix sorted (n-1)-grams sums.  Bare uint64_t array.   
 */
void SumExtend(const util::stream::ChainPosition &input, const util::stream::ChainPosition &output);

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_SUM_EXTEND__
