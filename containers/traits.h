#ifndef __CONTAINERS_TRAITS_H__
#define __CONTAINERS_TRAITS_H__

#include "listnodes.h"
#include "GeneralNode.h"
#include <functional>

template <typename T, typename NodeType, typename Comparator>
struct ListTraits {
  using value_type = T;
  using Node = NodeType;
  using ForwardIterator = LinkedListForwardIterator<T, Node>;
  using Compare = Comparator;
};

template <typename T, typename _Compare = std::less<T>>
struct LinkedListTraits : ListTraits<T, LinkedListNode<T>, _Compare> {};

template <typename T, typename _Compare = std::less<T>>
struct DoubleLinkedListTraits : ListTraits<T, DoubleLinkedNode<T>, _Compare> {
  using BackwardIterator = DoubleLinkedListBackwardIterator<T>;
};

template <class T>
using LinkedListAscTraits = LinkedListTraits<T, std::less<T>>;

template <class T>
using LinkedListDescTraits = LinkedListTraits<T, std::greater<T>>;

template <class T>
using DoubleLinkedListAscTraits = DoubleLinkedListTraits<T, std::less<T>>;

template <class T>
using DoubleLinkedListDescTraits = DoubleLinkedListTraits<T, std::greater<T>>;

template <typename T, typename Comparator = std::less<T>>
struct HeapTraits {
  using value_type = T;
  using Node = GeneralNode<T>;
  using compare_type = Comparator;
};

template <typename T>
struct MinHeapTraits : HeapTraits<T, std::less<T>> {};

template <typename T>
struct MaxHeapTraits : HeapTraits<T, std::greater<T>> {};

#endif // __CONTAINERS_TRAITS_H__
