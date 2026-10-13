#ifndef CONTAINERS_ALIASES_H
#define CONTAINERS_ALIASES_H

#include "vector.h"
#include "traits.h"
#include "circularlinkedlist.h"
#include "circulardoublelinkedlist.h"

template <typename T>
using VectorAsc = Vector<VectorAscTraits<T>>;

template <typename T>
using LinkedAscList = LinkedList<LinkedListAscTraits<T>>;

template <typename T>
using LinkedDescList = LinkedList<LinkedListDescTraits<T>>;

template <typename T>
using CircularLinkedAscList = CircularLinkedList<LinkedListAscTraits<T>>;

template <typename T>
using CircularLinkedDescList = CircularLinkedList<LinkedListDescTraits<T>>;

template <typename T>
using DoubleLinkedAscList = DoubleLinkedList<DoubleLinkedListAscTraits<T>>;

template <typename T>
using DoubleLinkedDescList = DoubleLinkedList<DoubleLinkedListDescTraits<T>>;

template <typename T>
using CircularDoubleLinkedAscList = CircularDoubleLinkedList<DoubleLinkedListAscTraits<T>>;

template <typename T>
using CircularDoubleLinkedDescList = CircularDoubleLinkedList<DoubleLinkedListDescTraits<T>>;

#endif
