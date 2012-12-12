#ifndef LM_BUILDER_SORT__
#define LM_BUILDER_SORT__

#include <functional>

namespace lm {
namespace builder {

template <class Child> class Comparator : public std::binary_function<const void *, const void *, bool> {
  public:
    explicit Comparator(std::size_t order) : order_(order) {}

    inline bool operator()(const void *lhs, const void *rhs) const {
      return (*static_cast<Child*>(this))(static_cast<const WordIndex*>(lhs), static_cast<const WordIndex*>(rhs));
    }

  protected:
    std::size_t order_;
};

class SuffixOrder : public Comparator<SuffixOrder> {
  public:
    explicit SuffixOrder(std::size_t order) : Comparator<SuffixOrder>(order) {}

    inline bool operator()(const WordIndex *lhs, const WordIndex *rhs) const {
      for (std::size_t i = order_ - 1; i != 0; --i) {
        if (lhs[i] != rhs[i])
          return lhs[i] < rhs[i];
      }
      return lhs[0] < rhs[0];
    }
};

class ContextOrder : public Comparator<SuffixOrder> {
  public:
    explicit ContextOrder(std::size_t order) : Comparator<SuffixOrder>(order) {}

    inline bool operator()(const WordIndex *lhs, const WordIndex *rhs) const {
      for (int i = order_ - 2; i >= 0; --i) {
        if (lhs[i] != rhs[i])
          return lhs[i] < rhs[i];
      }
      return lhs[order_ - 1] < rhs[order_ - 1];
    }
};

} // namespace builder
} // namespace lm

#endif // LM_BUILDER_SORT__
