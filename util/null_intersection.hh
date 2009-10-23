#ifndef UTIL_NULL_INTERSECTION
#define UTIL_NULL_INTERSECTION

#include <algorithm>
#include <functional>
#include <vector>

namespace util {

template <class Iterator> struct BeginEnd {
	typedef typename std::iterator_traits<Iterator>::value_type Value;
	BeginEnd(const Iterator &in_begin, const Iterator &in_end) : begin(in_begin), end(in_end) {}

	template <unsigned size> BeginEnd(const Value (&values)[size]) : begin(values), end(values + size) {}
	BeginEnd(const std::vector<Value> &vec) : begin(vec.begin()), end(vec.end()) {}

	Iterator begin;
	Iterator end;
};

template <class Iterator> BeginEnd<Iterator> MakeBeginEnd(const Iterator &in_begin, const Iterator &in_end) {
	return BeginEnd<Iterator>(in_begin, in_end);
}

namespace detail {
template <class Iterator> struct BeginEndLessBySize : public std::binary_function<const BeginEnd<Iterator> &, const BeginEnd<Iterator> &, bool> {
  bool operator()(const BeginEnd<Iterator> &left, const BeginEnd<Iterator> &right) const {
		return (left.end - left.begin) < (right.end - right.end);
	}
};
} // namespace detail

/* Takes sets specified by their iterators and returns true iff they intersect.  
 * Each set must be sorted in increasing order.
 * sets is left in an undefined state.
 */
template <class Iterator, class Less> bool NullIntersection(std::vector<BeginEnd<Iterator> > &sets, const Less &less) {
	typedef std::vector<BeginEnd<Iterator> > Sets;
	if (sets.empty()) return false;
	std::sort(sets.begin(), sets.end(), detail::BeginEndLessBySize<Iterator>());

	if (sets.front().begin == sets.front().end) return true;
	Iterator highest(sets.front().begin);
	for (typename Sets::iterator i(sets.begin()); i != sets.end(); ) {
		i->begin = std::lower_bound(i->begin, i->end, *highest, less);
		if (i->begin == i->end) return true;
		if (*(i->begin) > *highest) {
			highest = i->begin;
			// start over
			i = sets.begin();
		} else {
			++i;
		}
	}
	return false;
}

template <class Iterator> bool NullIntersection(std::vector<BeginEnd<Iterator> > &sets) {
	typedef std::less<typename std::iterator_traits<Iterator>::value_type> Less;
  return NullIntersection<Iterator, Less>(sets, Less());
}

} // namespace util

#endif // UTIL_NULL_INTERSECTION
