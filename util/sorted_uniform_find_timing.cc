#include "util/sorted_uniform_find.hh"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>
#include <boost/unordered_set.hpp>

#include <ext/hash_set>

#include <algorithm>
#include <iostream>
#include <limits>
#include <set>
#include <vector>

#include <time.h>

namespace {

struct PivotWide {
	inline size_t operator()(const uint64_t off, const uint64_t span, const uint64_t diff) const {
	  typedef unsigned int local_uint128_t __attribute__((mode(TI)));
	  return static_cast<size_t>(((local_uint128_t)off * (span)) / (diff));
	}
};

struct PivotFloat {
	inline size_t operator()(const uint64_t off, const uint64_t span, const uint64_t diff) const {
		size_t ret = static_cast<size_t>(static_cast<float>(off) * static_cast<float>(span) / static_cast<float>(diff));
		return (ret <= span) ? ret : span;
	}
};

struct Entry {
  uint64_t off, span, diff;
};

template <class Pivot> clock_t TimePivot(const std::vector<Entry> &entries, size_t &sum_out) {
	clock_t start = clock();
	size_t sum = 0;
	Pivot pivot;
	for (std::vector<Entry>::const_iterator i = entries.begin(); i != entries.end(); ++i) {
		sum += pivot(i->off, i->span, i->diff);
	}
	sum_out += sum;
	return clock() - start;
}

void TimePivots() {
  boost::mt19937 rng;
  boost::uniform_int<uint64_t> range(0, std::numeric_limits<uint64_t>::max());
  boost::variate_generator<boost::mt19937&, boost::uniform_int<uint64_t> > gen(rng, range);

	std::vector<Entry> entries;
	Entry tmp;
	for (size_t i = 0; i < 10000000; ++i) {
		tmp.diff = gen();
		tmp.off = gen() % tmp.diff;
		tmp.span = gen();
		entries.push_back(tmp);
	}

	size_t sum_out = 0;
	std::cout << "Float pivot:   " << TimePivot<PivotFloat>(entries, sum_out) << std::endl;
	std::cout << "Integer pivot: " << TimePivot<PivotWide>(entries, sum_out) << std::endl;
	std::cout << "Checksum: " << sum_out << std::endl;
}

bool Found(const __gnu_cxx::hash_set<uint64_t> &lookup, uint64_t key) {
	return lookup.find(key) != lookup.end();
}
bool Found(const boost::unordered_set<uint64_t> &lookup, uint64_t key) {
	return lookup.find(key) != lookup.end();
}
bool Found(const std::set<uint64_t> &lookup, uint64_t key) {
	return lookup.find(key) != lookup.end();
}
bool Found(const std::vector<uint64_t> &lookup, uint64_t key) {
	return 0 != util::SortedUniformFind(&*lookup.begin(), &*(lookup.end() - 1), key);
}

template <class L> size_t TimeTable(const L &lookup, const std::vector<uint64_t> &hits, const std::vector<uint64_t> &misses) {
	size_t count = 0;
	clock_t start = clock();
	for (std::vector<uint64_t>::const_iterator i = hits.begin(); i != hits.end(); ++i) {
		if (Found(lookup, *i)) ++count;
	}
	clock_t middle = clock();
	for (std::vector<uint64_t>::const_iterator i = misses.begin(); i != misses.end(); ++i) {
		if (Found(lookup, *i)) ++count;
	}
	clock_t finish = clock();
	std::cout << (middle - start) << ' ' << (finish - middle);
	return count;
}

// This tests unique values.
void TimeLookup(const size_t entries) {
	const size_t kHits = 1000000, kMisses = 1000000;
  boost::mt19937 rng;
  boost::uniform_int<uint64_t> range(0, std::numeric_limits<uint64_t>::max());
  boost::variate_generator<boost::mt19937&, boost::uniform_int<uint64_t> > gen(rng, range);

	__gnu_cxx::hash_set<uint64_t> gnu_hash_set;
	boost::unordered_set<uint64_t> boost_unordered_set;
	std::set<uint64_t> std_set;

	std::vector<uint64_t> sorted;
	sorted.reserve(entries);

	while (gnu_hash_set.size() < entries) {
		const uint64_t val = gen();
		if (gnu_hash_set.insert(val).second) {
			boost_unordered_set.insert(val);
			std_set.insert(val);
			sorted.push_back(val);
		}
	}
	std::sort(sorted.begin(), sorted.end());

	std::vector<uint64_t> hits;
	hits.reserve(kHits);
	boost::uniform_int<uint64_t> hit_range(0, entries - 1);
  boost::variate_generator<boost::mt19937&, boost::uniform_int<uint64_t> > gen_hit(rng, hit_range);
	for (size_t i = 0; i < kHits; ++i) {
		hits.push_back(sorted[gen_hit()]);
	}

	std::vector<uint64_t> misses;
	misses.reserve(kMisses);
	while (misses.size() < kMisses) {
		uint64_t val = gen();
		if (gnu_hash_set.find(val) == gnu_hash_set.end()) {
			misses.push_back(val);
		}
	}

	size_t total = 0;
	std::cout << entries << ' ';
	total += TimeTable(gnu_hash_set, hits, misses);
	std::cout << ' ';
	total += TimeTable(boost_unordered_set, hits, misses);
	std::cout << ' ';
	total += TimeTable(std_set, hits, misses);
	std::cout << ' ';
	total += TimeTable(sorted, hits, misses);
	std::cout << std::endl;
}

} // namespace

int main() {
	//TimePivots();
	for (size_t i = 1; i <= 33554432; i <<= 1) {
  	TimeLookup(i);
	}
}
