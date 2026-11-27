#ifndef CONTAINERS_ALIASES_H
#define CONTAINERS_ALIASES_H

#include "../types.h"
#include "trees/binarytree.h"
#include "trees/avl.h"
#include "hashing/hashtable.h"
#include "trees/Heap.h"
#include "lists/vector.h"
#include "lists/array.h"
#include "core/traits.h"
#include "lists/linkedlist.h"
#include "trees/merkletree.h"
#include "trees/BTree.h"
#include "trees/BPlusTree.h"
#include "trees/RTree.h"
#include "graphs/Graph.h"
#include "spreadsheet/exprtree.h"
#include "spreadsheet/spreadsheet.h"
#include <functional>

/** Vector de elementos de tipo @p T. */
template <typename T>
using VectorOf = Vector<VectorTraits<T>>;

/** Arreglo dinámico de elementos de tipo @p T, sin nodo ni mutex. */
template <typename T>
using ArrayOf = Array<ArrayTraits<T>>;

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

/** Tabla clave-valor ordenada según @p Compare, con un AVL por dentro. */
template <typename Key, typename Value, typename Compare = std::less<Key>>
using HashTableOf = HashTable<HashTableTraits<Key, Value, Compare>>;

/** Árbol de Merkle sobre bloques de tipo @p T con el hasher indicado. */
template <typename T, typename Hasher = StdMerkleHasher<T>>
using MerkleOf = MerkleTree<MerkleTreeTraits<T, Hasher>>;

/** Árbol B* ordenado según @p Compare, con páginas de @p Order claves. */
template <typename T, typename Compare = std::less<T>, std::size_t Order = 3>
using BTreeOf = BTree<BTreeTraits<T, Compare, Order>>;

/** Árbol B+ ordenado según @p Compare, con nodos de @p Order claves. */
template <typename T, typename Compare = std::less<T>, std::size_t Order = 4>
using BPlusTreeOf = BPlusTree<BPlusTreeTraits<T, Compare, Order>>;

/** Grafo ponderado con vértices identificados por claves de tipo @p V y pesos de tipo @p W. */
template <typename V, typename W = Real, typename Compare = std::less<V>>
using GraphOf = Graph<GraphTraits<V, W, Compare>>;

/** Árbol R de rectángulos con coordenadas de tipo @p Coordinate y nodos de @p Order entradas. */
template <typename Coordinate = Real, std::size_t Order = 4>
using RTreeOf = RTree<RTreeTraits<Coordinate, Order>>;

/** Árbol de sintaxis de una fórmula con resultados de tipo @p Number. */
template <typename Number = Real>
using ExprTreeOf = ExprTree<ExprTreeTraits<Number>>;

/** Hoja de cálculo de texto con resultados de tipo @p Number. */
template <typename Number = Real>
using SheetOf = Sheet<SheetTraits<Number>>;

#endif
