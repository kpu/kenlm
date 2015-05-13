#ifndef LM_INTERPOLATE_BOUNDED_SEQUENCE_ITERATOR_H
#define LM_INTERPOLATE_BOUNDED_SEQUENCE_ITERATOR_H

/* A custom iterator
 */

#include <cstring>
#include <iostream>
#include <iterator>

namespace lm {
namespace interpolate {

class BoundedSequenceIteratorEncoder : public std::iterator<std::input_iterator_tag, const unsigned char> {
  const unsigned char* p;
  public:
    BoundedSequenceIteratorEncoder(const unsigned char *x) :p(x) {}
    BoundedSequenceIteratorEncoder(const BoundedSequenceIteratorEncoder& bsi) : p(bsi.p) {}
    BoundedSequenceIteratorEncoder& operator++() {++p; return *this;}
    BoundedSequenceIteratorEncoder operator++(int) {BoundedSequenceIteratorEncoder tmp(*this); operator++(); return tmp;}
    bool operator==(const BoundedSequenceIteratorEncoder& rhs) {return p==rhs.p;}
    bool operator!=(const BoundedSequenceIteratorEncoder& rhs) {return p!=rhs.p;}
    const unsigned char& operator*() {return *p;}


};

class BoundedSequenceIteratorDecoder : public std::iterator<std::input_iterator_tag, uint8_t> {
  uint8_t* p;
  public:
    BoundedSequenceIteratorDecoder(uint8_t *x) : p(x) {}
    BoundedSequenceIteratorDecoder(const BoundedSequenceIteratorDecoder& bsi) : p(bsi.p) {}
    BoundedSequenceIteratorDecoder& operator++() {++p; return *this;}
    BoundedSequenceIteratorDecoder operator++(int) {BoundedSequenceIteratorDecoder tmp(*this); operator++(); return tmp;}
    bool operator==(const BoundedSequenceIteratorDecoder& rhs) {return p==rhs.p;}
    bool operator!=(const BoundedSequenceIteratorDecoder& rhs) {return p!=rhs.p;}
    uint8_t& operator*() {return *p;}


};

}} // namespaces

#endif // LM_INTERPOLATE_BOUNDED_SEQUENCE_ITERATOR_H
