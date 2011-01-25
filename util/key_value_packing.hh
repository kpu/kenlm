#ifndef UTIL_KEY_VALUE_PACKING__
#define UTIL_KEY_VALUE_PACKING__

/* Why such a general interface?  I'm planning on doing bit-level packing. */

#include <algorithm>
#include <cstddef>
#include <cstring>

#include <inttypes.h>

namespace util {

template <class Key, class Value> struct Entry {
  Key key;
  Value value;

  const Key &GetKey() const { return key; }
  const Value &GetValue() const { return value; }

  Value &MutableValue() { return value; }

  void Set(const Key &key_in, const Value &value_in) {
    SetKey(key_in);
    SetValue(value_in);
  }
  void SetKey(const Key &key_in) { key = key_in; }
  void SetValue(const Value &value_in) { value = value_in; }

  bool operator<(const Entry<Key, Value> &other) const { return GetKey() < other.GetKey(); }
};

// And now for a brief interlude to specialize std::swap.  
} // namespace util
namespace std {
template <class Key, class Value> void swap(util::Entry<Key, Value> &first, util::Entry<Key, Value> &second) {
  swap(first.key, second.key);
  swap(first.value, second.value);
}
}// namespace std
namespace util {

template <class KeyT, class ValueT> class AlignedPacking {
  public:
    typedef KeyT Key;
    typedef ValueT Value;
    
  public:
    static const std::size_t kBytes = sizeof(Entry<Key, Value>);
    static const std::size_t kBits = kBytes * 8;

    typedef Entry<Key, Value> * MutableIterator;
    typedef const Entry<Key, Value> * ConstIterator;
    typedef const Entry<Key, Value> & ConstReference;

    static MutableIterator FromVoid(void *start) {
      return reinterpret_cast<MutableIterator>(start);
    }

    static Entry<Key, Value> Make(const Key &key, const Value &value) {
      Entry<Key, Value> ret;
      ret.Set(key, value);
      return ret;
    }
};

template <class KeyT, class ValueT> class ByteAlignedPacking {
  public:
    typedef KeyT Key;
    typedef ValueT Value;

  private:
#pragma pack(push)
#pragma pack(1)
    struct RawEntry {
      Key key;
      Value value;

      const Key &GetKey() const { return key; }
      const Value &GetValue() const { return value; }

      Value &MutableValue() { return value; }

      void Set(const Key &key_in, const Value &value_in) {
        SetKey(key_in);
        SetValue(value_in);
      }
      void SetKey(const Key &key_in) { key = key_in; }
      void SetValue(const Value &value_in) { value = value_in; }

      bool operator<(const RawEntry &other) const { return GetKey() < other.GetKey(); }
    };
#pragma pack(pop)

    friend void std::swap<>(RawEntry&, RawEntry&);

  public:
    typedef RawEntry *MutableIterator;
    typedef const RawEntry *ConstIterator;
    typedef RawEntry &ConstReference;

    static const std::size_t kBytes = sizeof(RawEntry);
    static const std::size_t kBits = kBytes * 8;

    static MutableIterator FromVoid(void *start) {
      return MutableIterator(reinterpret_cast<RawEntry*>(start));
    }

    static RawEntry Make(const Key &key, const Value &value) {
      RawEntry ret;
      ret.Set(key, value);
      return ret;
    }
};

} // namespace util
namespace std {
template <class Key, class Value> void swap(
    typename util::ByteAlignedPacking<Key, Value>::RawEntry &first,
    typename util::ByteAlignedPacking<Key, Value>::RawEntry &second) {
  swap(first.key, second.key);
  swap(first.value, second.value);
}
}// namespace std

#endif // UTIL_KEY_VALUE_PACKING__
