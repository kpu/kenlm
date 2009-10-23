#include "util/null_intersection.hh"

#define BOOST_TEST_MODULE NullIntersectionTest
#include <boost/test/unit_test.hpp>

namespace util {
namespace {

BOOST_AUTO_TEST_CASE(Empty) {
	std::vector<BeginEnd<const unsigned int*> > sets;
	BOOST_CHECK(!NullIntersection(sets));
	
	sets.push_back(BeginEnd<const unsigned int*>(NULL, NULL));
	BOOST_CHECK(NullIntersection(sets));
}

BOOST_AUTO_TEST_CASE(Single) {
	std::vector<unsigned int> nums;
	nums.push_back(1);
	nums.push_back(4);
	nums.push_back(100);
	std::vector<BeginEnd<std::vector<unsigned int>::const_iterator> > sets;
	sets.push_back(nums);

	BOOST_CHECK(!NullIntersection(sets));
}

BOOST_AUTO_TEST_CASE(MultiNone) {
	unsigned int nums0[] = {1, 3, 4, 22};
	unsigned int nums1[] = {2, 5, 12};
	unsigned int nums2[] = {4, 17};

	std::vector<BeginEnd<const unsigned int*> > sets;
	sets.push_back(nums0);
	sets.push_back(nums1);
	sets.push_back(nums2);

	BOOST_CHECK(NullIntersection(sets));
}

BOOST_AUTO_TEST_CASE(MultiOne) {
	unsigned int nums0[] = {1, 3, 4, 17, 22};
	unsigned int nums1[] = {2, 5, 12, 17};
	unsigned int nums2[] = {4, 17};

	std::vector<BeginEnd<const unsigned int*> > sets;
	sets.push_back(nums0);
	sets.push_back(nums1);
	sets.push_back(nums2);

	BOOST_CHECK(!NullIntersection(sets));
}

} // namespace
} // namespace util
