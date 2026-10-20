#ifndef CONTAINERS_HEAPTRAITS_H
#define CONTAINERS_HEAPTRAITS_H

#include "GeneralNode.h"
#include <functional>

template <typename Node> class HeapConstIterator;

template <typename T, typename Compare = std::less<T>> struct HeapTraits {
    using value_type = T;
    using compare_type = Compare;
    using Node = GeneralNode<T>;
    using ForwardIterator = HeapConstIterator<Node>;
};

template <typename T> using MinHeapTraits = HeapTraits<T, std::less<T>>;

template <typename T> using MaxHeapTraits = HeapTraits<T, std::greater<T>>;

#endif // CONTAINERS_HEAPTRAITS_H
