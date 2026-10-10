#ifndef __CONTAINERS_TRAITS_H__
#define __CONTAINERS_TRAITS_H__

#include "doublelinkedlist.h"
#include "linkedlist.h"
template <typename T, typename _Compare = std::less<T>>
struct LinkedListTraits {
  using value_type = T;
  using Node = LinkedListNode<T>;
  using ForwardIterator = LinkedListForwardIterator<T, Node>;
  using Compare = _Compare;
};

template <typename T, typename _Compare = std::less<T>>
struct DoubleLinkedListTraits : LinkedListTraits<T, _Compare> {
  using Node = DoubleLinkedNode<T>;
  using ForwardIterator = LinkedListForwardIterator<T, Node>;
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

#endif // __CONTAINERS_TRAITS_H__
