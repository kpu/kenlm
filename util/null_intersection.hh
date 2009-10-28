#ifndef UTIL_NULL_INTERSECTION
#define UTIL_NULL_INTERSECTION

#include "boost/range/iterator_range.hpp"

#include <algorithm>
#include <functional>
#include <vector>

namespace util {

namespace detail {
template <class Range> struct RangeLessBySize : public std::binary_function<const Range &, const Range &, bool> {
  bool operator()(const Range &left, const Range &right) const {
		return left.size() < right.size();
	}
};
} // namespace detail

/* Takes sets specified by their iterators and returns true iff they intersect.  
 * Each set must be sorted in increasing order.
 * sets is left in an undefined state.
 */
template <class Iterator, class Less> bool NullIntersection(std::vector<boost::iterator_range<Iterator> > &sets, const Less &less) {
	typedef std::vector<boost::iterator_range<Iterator> > Sets;
	if (sets.empty()) return false;
	std::sort(sets.begin(), sets.end(), detail::RangeLessBySize<boost::iterator_range<Iterator> >());

	if (sets.front().empty()) return true;
  // Possibly suboptimal to copy; makes unsigned int go slighly faster.  
	typename std::iterator_traits<Iterator>::value_type highest(sets.front().front());
	for (typename Sets::iterator i(sets.begin()); i != sets.end(); ) {
		i->advance_begin(std::lower_bound(i->begin(), i->end(), highest, less) - i->begin());
		if (i->empty()) return true;
		if (i->front() > highest) {
			highest = i->front();
			// start over
			i = sets.begin();
		} else {
			++i;
		}
	}
	return false;
}

template <class Iterator> bool NullIntersection(std::vector<boost::iterator_range<Iterator> > &sets) {
	typedef std::less<typename std::iterator_traits<Iterator>::value_type> Less;
  return NullIntersection<Iterator, Less>(sets, Less());
}

} // namespace util

#endif // UTIL_NULL_INTERSECTION
