#ifndef __CONTAINERS_TRAITS_H__
#define __CONTAINERS_TRAITS_H__

#include "listnodes.h"
#include "GeneralNode.h"
#include "binarytreeiterator.h"
#include "traversalops.h"
#include <functional>

template <typename T, bool Reverse> class VectorIterator;
template <typename Traits, typename Ops = FullTraversalOps> class Vector;
template <typename T, typename Derived> class BinaryTreeNode;
template <typename T> class AVLNode;

template <typename T> struct VectorTraits {
    using value_type = T;
    using Node = GeneralNode<T>;
    using ForwardIterator = VectorIterator<Node, false>;
    using BackwardIterator = VectorIterator<Node, true>;
};

template <typename T, typename Comparator = std::less<T>,
          typename NodeType = BinaryTreeNode<T, void>>
struct BinaryTreeTraits {
    using value_type = T;
    using Node = NodeType;
    using Compare = Comparator;
    using ForwardIterator = BinaryTreeIterator<const Node>;
    using BackwardIterator = BinaryTreeIterator<const Node, TreeOrder::InOrder, true>;
};

template <typename T, typename Comparator = std::less<T>>
using AVLTraits = BinaryTreeTraits<T, Comparator, AVLNode<T>>;

template <typename T, typename NodeType, typename Comparator> struct ListTraits {
    using value_type = T;
    using Node = NodeType;
    using ForwardIterator = LinkedListForwardIterator<T, Node>;
    using Compare = Comparator;
};

template <typename T, typename Comparator = std::less<T>>
struct LinkedListTraits : ListTraits<T, LinkedListNode<T>, Comparator> {};

template <typename T, typename Comparator = std::less<T>>
struct DoubleLinkedListTraits : ListTraits<T, DoubleLinkedNode<T>, Comparator> {
    using BackwardIterator = DoubleLinkedListBackwardIterator<T>;
};

template <typename T, typename Comparator = std::less<T>>
struct HeapTraits {
    using value_type = T;
    using Storage = Vector<VectorTraits<T>>;
    using Node = typename Storage::Node;
    using Compare = Comparator;
    using ForwardIterator = typename Storage::ConstForwardIterator;
    using BackwardIterator = typename Storage::ConstBackwardIterator;
};

#endif // __CONTAINERS_TRAITS_H__
