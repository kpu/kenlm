#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/random/variate_generator.hpp>

#include <iostream>
#include <limits>

#include <time.h>

struct PivotWide {
	inline size_t operator()(const uint64_t off, const uint64_t span, const uint64_t diff) const {
	  typedef unsigned int local_uint128_t __attribute__((mode(TI)));
	  return static_cast<size_t>(((local_uint128_t)off * (span)) / (diff));
	}
};

struct PivotFloat {
	inline size_t operator()(const uint64_t off, const uint64_t span, const uint64_t diff) const {
		return static_cast<size_t>(static_cast<float>(off) * static_cast<float>(span) / static_cast<float>(diff));
	}
};

template <class Pivot> clock_t Time(const size_t amount, size_t &sum_out) {
  boost::mt19937 rng;
  boost::uniform_int<uint64_t> range(0, std::numeric_limits<uint64_t>::max());
  boost::variate_generator<boost::mt19937&, boost::uniform_int<uint64_t> > gen(rng, range);
	Pivot pivot;
	clock_t start = clock();
	size_t sum = 0;
	for (size_t i = 0; i < amount; ++i) {
		sum += pivot(gen(), gen(), gen());
	}
	sum_out += sum;
	return clock() - start;
}

int main() {
	size_t sum_out = 0;
	std::cout << "Float implementation    " << Time<PivotFloat>(10000000, sum_out) << std::endl;
	std::cout << "Integer implementation  " << Time<PivotWide>(10000000, sum_out) << std::endl;
	std::cout << "Ignore me: " << sum_out << std::endl;
}
