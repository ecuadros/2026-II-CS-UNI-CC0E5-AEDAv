#ifndef __CONTAINERS_TRAITS_H__
#define __CONTAINERS_TRAITS_H__

#include "listnodes.h"
#include "GeneralNode.h"
#include "treeiterator.h"
#include <functional>

template <typename T> class VectorForwardIterator;
template <typename T> class VectorBackwardIterator;
template <typename Traits> class Vector;
template <typename T> class BinaryTreeNode;

template <typename T> struct VectorAscTraits {
    using value_type = T;
    using Node = GeneralNode<T>;
    using ForwardIterator = VectorForwardIterator<Node>;
    using BackwardIterator = VectorBackwardIterator<Node>;
};

template <typename T> struct BinaryTreeTraits {
    using value_type = T;
    using Node = BinaryTreeNode<T>;
    using ForwardIterator = BinaryTreeIterator<const Node>;
    using BackwardIterator = BinaryTreeIterator<const Node, true>;
    using PreOrderIterator = BinaryTreeIterator<const Node, false, TreeTraversalOrder::PreOrder>;
    using PostOrderIterator = BinaryTreeIterator<const Node, false, TreeTraversalOrder::PostOrder>;
};



template <typename T, typename NodeType, typename Comparator> struct ListTraits {
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

template <class T> using LinkedListAscTraits = LinkedListTraits<T, std::less<T>>;

template <class T> using LinkedListDescTraits = LinkedListTraits<T, std::greater<T>>;

template <class T> using DoubleLinkedListAscTraits = DoubleLinkedListTraits<T, std::less<T>>;

template <class T> using DoubleLinkedListDescTraits = DoubleLinkedListTraits<T, std::greater<T>>;

template <typename T, typename Comparator>
struct HeapTraits {
    using value_type = T;
    using Storage = Vector<VectorAscTraits<T>>;
    using Node = typename Storage::Node;
    using compare_type = Comparator;
    using ForwardIterator = typename Storage::ConstForwardIterator;
    using BackwardIterator = typename Storage::ConstBackwardIterator;
};

template <typename T>
using MinHeapTraits = HeapTraits<T, std::less<T>>;

template <typename T>
using MaxHeapTraits = HeapTraits<T, std::greater<T>>;

#endif // __CONTAINERS_TRAITS_H__
