#ifndef UTIL_LOG_NUM__
#define UTIL_LOG_NUM__

#include <cmath>
#include <limits>
#include <iostream>

#include <assert.h>

struct AlreadyLogTag {};

// Store a non-negative number in log format.  The template argument is the
// underlying storage format, such as float, double, or long double.  
// The class can be used like a normal number i.e. the multiplication operator
// internally adds log values.  
template <class Num> class LogNum {
 public:
  // Default initialize to zero.
  LogNum() : log_value_(-std::numeric_limits<Num>::infinity()) {}

  // From linear.
  explicit LogNum(const Num &value) {
    assert(value >= Num());
    log_value_ = log(value);
  }

  LogNum(AlreadyLogTag ignored, const Num &log_value) : log_value_(log_value) {
    assert(!std::isnan(log_value));
  }

  LogNum(const LogNum<Num> &to) : log_value_(to.Log()) {}
  
  LogNum<Num> &operator=(const Num &linear_value) {
    assert(linear_value >= Num());
    log_value_ = log(linear_value);
    return *this;
  }

  LogNum<Num> &operator=(const LogNum<Num> &to) {
    log_value_ = to.Log();
    return *this;
  }

  LogNum<Num> &operator*=(const LogNum<Num> &value) {
    log_value_ += value.Log();
    return *this;
  }

  LogNum<Num> &operator/=(const LogNum<Num> &value) {
    log_value_ -= value.Log();
    return *this;
  }

  // This could be operator+ but there are two reasons not to:
  // First, we're porting code from raw log format numbers which use + to mean multiply.  We want to catch these.  
  // Second, this addition is unstable and should only be used to add a few values.
  LogNum<Num> &UnstableAdd(const LogNum<Num> &right) {
    if (right.Log() < log_value_) {
      log_value_ += log1p(exp(right.Log() - log_value_));
    } else if (right.Log() > log_value_) {
      log_value_ = right.Log() + log1p(exp(log_value_ - right.Log()));
    } else {
      log_value_ += M_LN2;
    }
    return *this;
  }

  // Take the value to the power of a non-negative exponent.
  LogNum<Num> &PowEquals(const Num &exponent) {
    assert(exponent >= Num());
    log_value_ *= exponent;
    return *this;
  }

  Num Linear() const { return exp(log_value_); }
  // This is the same as operator= but here for completeness.
  void SetLinear(const Num &linear_value) { *this = linear_value; }

  Num Log() const { return log_value_; }

  Num Log10() const { return log_value_ * M_LOG10E; }

  void SetLog(const Num &log_value) { log_value_ = log_value; }

  void SetLog10(const Num &log10_value) { log_value_ = log10_value / M_LOG10E; }

  Num &MutableLog() { return log_value_; }

 private:
  // Natural log of value.
  Num log_value_;
};

template <class Num> LogNum<Num> operator*(const LogNum<Num> &left, const LogNum<Num> &right) {
  return LogNum<Num>(left) *= right;
}
template <class Num> LogNum<Num> operator/(const LogNum<Num> &left, const LogNum<Num> &right) {
  return LogNum<Num>(left) /= right;
}
// There is no operator+.  Use UnstableAdd and read the warning.

template <class Num> bool operator<(const LogNum<Num> &left, const LogNum<Num> &right) {
  return left.Log() < right.Log();
}
template <class Num> bool operator<=(const LogNum<Num> &left, const LogNum<Num> &right) {
  return left.Log() <= right.Log();
}
template <class Num> bool operator>(const LogNum<Num> &left, const LogNum<Num> &right) {
  return left.Log() > right.Log();
}
template <class Num> bool operator>=(const LogNum<Num> &left, const LogNum<Num> &right) {
  return left.Log() >= right.Log();
}
template <class Num> bool operator==(const LogNum<Num> &left, const LogNum<Num> &right) {
  return left.Log() == right.Log();
}
template <class Num> bool operator!=(const LogNum<Num> &left, const LogNum<Num> &right) {
  return left.Log() != right.Log();
}

// Notice this returns Num, not Log<Num>.
template <class Num> Num log(const LogNum<Num> &value) {
  return value.Log();
}
template <class Num> Num log10(const LogNum<Num> &value) {
  return value.Log10();
}
// exponent is a plain Num and must be non-negative.
template <class Num> LogNum<Num> pow(const LogNum<Num> &value, const Num &exponent) {
  return LogNum<Num>(value).PowEquals(exponent);
}
// exponent is a plain Num and must be non-negative.
template <class Num> LogNum<Num> pow(const LogNum<Num> &value, const LogNum<Num> &exponent) {
  return LogNum<Num>(value).PowEquals(exponent.Linear());
}

template <class Num> std::ostream &operator<<(std::ostream &stream, const LogNum<Num> &value) {
  return stream << value.Linear();
}

template <class Num> std::istream &operator>>(std::istream &stream, LogNum<Num> &value) {
  Num tmp;
  stream >> tmp;
  value = tmp;
  return stream;
}

typedef LogNum<double> LogDouble;
typedef LogNum<float> LogFloat;
typedef LogNum<long double> LogLongDouble;

#endif // UTIL_LOG_NUM__
