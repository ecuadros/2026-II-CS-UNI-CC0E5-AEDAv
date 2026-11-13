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

/** Vector de elementos de tipo @p T. */
template <typename T>
using VectorOf = Vector<VectorTraits<T>>;

/** Lista simplemente enlazada ordenada según @p Compare. */
template <typename T, typename Compare = std::less<T>>
using SortedLinkedList = LinkedList<LinkedListTraits<T, Compare>>;

/** Lista circular simplemente enlazada ordenada según @p Compare. */
template <typename T, typename Compare = std::less<T>>
using SortedCircularLinkedList = CircularLinkedList<LinkedListTraits<T, Compare>>;

/** Lista doblemente enlazada ordenada según @p Compare. */
template <typename T, typename Compare = std::less<T>>
using SortedDoubleLinkedList = DoubleLinkedList<DoubleLinkedListTraits<T, Compare>>;

/** Lista circular doblemente enlazada ordenada según @p Compare. */
template <typename T, typename Compare = std::less<T>>
using SortedCircularDoubleLinkedList = CircularDoubleLinkedList<DoubleLinkedListTraits<T, Compare>>;

/** Árbol binario de búsqueda sin balanceo. */
template <typename T, typename Compare = std::less<T>>
using BinarySearchTree = BinaryTree<BinaryTreeTraits<T, Compare>>;

/** Árbol AVL (binario de búsqueda autobalanceado). */
template <typename T, typename Compare = std::less<T>>
using AVLSearchTree = AVLTree<AVLTraits<T, Compare>>;

/** Heap con el menor elemento en la raíz. */
template <typename T>
using MinHeap = Heap<HeapTraits<T, std::less<T>>>;

/** Heap con el mayor elemento en la raíz. */
template <typename T>
using MaxHeap = Heap<HeapTraits<T, std::greater<T>>>;

/** Árbol de Merkle sobre bloques de tipo @p T con el hasher indicado. */
template <typename T, typename Hasher = StdMerkleHasher<T>>
using MerkleOf = MerkleTree<MerkleTreeTraits<T, Hasher>>;

#endif
