#ifndef TRAITS_H
#define TRAITS_H

#include <functional>
#include <istream>

template <typename T, typename _Compare>
struct DefaultTraits {
    using value_type = T;
    using Compare = _Compare;
    using Delim = std::istream::char_type;
};

template <typename T, typename _Compare = std::less<T>>
struct AscendingTraits : public DefaultTraits<T, _Compare> {};

template <typename T, typename _Compare = std::greater<T>>
struct DescendingTraits : public DefaultTraits<T, _Compare> {};

#endif