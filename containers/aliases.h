#ifndef CONTAINERS_ALIASES_H
#define CONTAINERS_ALIASES_H

#include "binarytree.h"
#include "avl.h"
#include "Heap.h"
#include "vector.h"
#include "traits.h"
#include "linkedlist.h"
#include "merkletree.h"
#include <functional>

template <typename T>
using VectorOf = Vector<VectorTraits<T>>;

template <typename T, typename Compare = std::less<T>>
using SortedLinkedList = LinkedList<LinkedListTraits<T, Compare>>;

template <typename T, typename Compare = std::less<T>>
using SortedCircularLinkedList = CircularLinkedList<LinkedListTraits<T, Compare>>;

template <typename T, typename Compare = std::less<T>>
using SortedDoubleLinkedList = DoubleLinkedList<DoubleLinkedListTraits<T, Compare>>;

template <typename T, typename Compare = std::less<T>>
using SortedCircularDoubleLinkedList = CircularDoubleLinkedList<DoubleLinkedListTraits<T, Compare>>;

template <typename T, typename Compare = std::less<T>>
using BinarySearchTree = BinaryTree<BinaryTreeTraits<T, Compare>>;

template <typename T, typename Compare = std::less<T>>
using AVLSearchTree = AVLTree<AVLTraits<T, Compare>>;

template <typename T>
using MinHeap = Heap<HeapTraits<T, std::less<T>>>;

template <typename T>
using MaxHeap = Heap<HeapTraits<T, std::greater<T>>>;

template <typename T, typename Hasher = StdMerkleHasher<T>>
using MerkleOf = MerkleTree<MerkleTreeTraits<T, Hasher>>;

#endif
