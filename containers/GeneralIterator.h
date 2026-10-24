#ifndef __GENERAL_ITERATOR_H__
#define __GENERAL_ITERATOR_H__

#include <iostream>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <algorithm> // para std::swap

// CRTP
template <typename Derived, typename T>
class GeneralIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;
    using pointer           = value_type *;
    using reference         = value_type&;
protected:
    pointer m_ptr;
public:
    GeneralIterator(pointer ptr) : m_ptr(ptr) {}
    value_type& operator*() const { return *m_ptr; }
    pointer operator->() const { return m_ptr; }
    friend bool operator== (const Derived& a, const Derived& b) { return a.m_ptr == b.m_ptr; };
    friend bool operator!= (const Derived& a, const Derived& b) { return a.m_ptr != b.m_ptr; };
};

#endif // __GENERAL_ITERATOR_H__
