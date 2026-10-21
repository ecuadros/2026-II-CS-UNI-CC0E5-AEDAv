#ifndef __TRAITS_H__
#define __TRAITS_H__
#include "vector.h"
#include <functional>

template<typename T, typename _Compare>
struct DefaultTraits {
    using value_type = T;
    using Compare = _Compare;
};

template<typename T, typename _Compare = std::less<T>>
struct AscendingTraits : public DefaultTraits<T, _Compare> {};

template<typename T, typename _Compare = std::greater<T>>
struct DescendingTraits : public DefaultTraits<T, _Compare> {};

template<typename T, typename _Compare>
struct HeapTraits : public DefaultTraits<T, _Compare> {
    using Node = GeneralNode<T>;
    using Container = Vector<VectorAscTraits<T>>;
};

template<typename T>
struct MinHeapTraits : public HeapTraits<T, std::less<T>> {};

template<typename T>
struct MaxHeapTraits : public HeapTraits<T, std::greater<T>> {};

#endif