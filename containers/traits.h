#ifndef __CONTAINERS_TRAITS_H__
#define __CONTAINERS_TRAITS_H__

#include "listnodes.h"
#include "GeneralNode.h"
#include "binarytreeiterator.h"
#include "traversalops.h"
#include <functional>

template <typename T, bool Reverse> class VectorIterator;
template <typename Traits, typename Ops = FullTraversalOps> class Vector;
template <typename T, typename Derived = void, typename R = Ref> class BinaryTreeNode;
template <typename T, typename R = Ref> class AVLNode;

/**
 * Tipos que configuran un Vector: valor, nodo e iteradores.
 * @tparam T Tipo del valor.
 */
template <typename T> struct VectorTraits {
    using value_type = T;
    using Node = GeneralNode<T>;
    using ForwardIterator = VectorIterator<Node, false>;
    using BackwardIterator = VectorIterator<Node, true>;
};

/**
 * Tipos que configuran un árbol binario de búsqueda.
 * @tparam T Tipo de la clave.
 * @tparam Comparator Criterio de orden.
 * @tparam NodeType Tipo de nodo; cambiarlo permite reutilizar el árbol (por ejemplo, con AVLNode).
 */
template <typename T, typename Comparator = std::less<T>,
          typename NodeType = BinaryTreeNode<T, void>>
struct BinaryTreeTraits {
    using value_type = T;
    using Node = NodeType;
    using ref_type = typename NodeType::ref_type;
    using Compare = Comparator;
    using ForwardIterator = BinaryTreeIterator<const Node>;
    using BackwardIterator = BinaryTreeIterator<const Node, TreeOrder::InOrder, true>;
};

/**
 * Traits de un AVL: los de BinaryTreeTraits con AVLNode como nodo.
 * @tparam T Tipo de la clave.
 * @tparam Comparator Criterio de orden.
 * @tparam R Tipo de la referencia guardada junto a cada clave.
 */
template <typename T, typename Comparator = std::less<T>, typename R = Ref>
using AVLTraits = BinaryTreeTraits<T, Comparator, AVLNode<T, R>>;

/**
 * Tipos comunes a las listas enlazadas.
 * @tparam T Tipo del valor.
 * @tparam NodeType Tipo de nodo.
 * @tparam Comparator Criterio de orden.
 */
template <typename T, typename NodeType, typename Comparator> struct ListTraits {
    using value_type = T;
    using Node = NodeType;
    using ForwardIterator = LinkedListForwardIterator<T, Node>;
    using Compare = Comparator;
};

/** Traits de la lista simplemente enlazada. */
template <typename T, typename Comparator = std::less<T>>
struct LinkedListTraits : ListTraits<T, LinkedListNode<T>, Comparator> {};

/** Traits de la lista doblemente enlazada: añade el iterador hacia atrás. */
template <typename T, typename Comparator = std::less<T>>
struct DoubleLinkedListTraits : ListTraits<T, DoubleLinkedNode<T>, Comparator> {
    using BackwardIterator = DoubleLinkedListBackwardIterator<T>;
};

/**
 * Tipos que configuran un Heap, que guarda sus elementos en un Vector.
 * @tparam T Tipo del valor.
 * @tparam Comparator Criterio que decide qué elemento queda en la raíz.
 */
template <typename T, typename Comparator = std::less<T>>
struct HeapTraits {
    using value_type = T;
    using Storage = Vector<VectorTraits<T>>;
    using Node = typename Storage::Node;
    using Compare = Comparator;
    using ForwardIterator = typename Storage::ConstForwardIterator;
    using BackwardIterator = typename Storage::ConstBackwardIterator;
};

template <typename Digest> class MerkleNode;
template <typename T> struct StdMerkleHasher;

/**
 * Tipos que configuran un MerkleTree.
 * @tparam T Tipo de los bloques de datos.
 * @tparam HasherType Política de hash, con `Digest`, `leaf()` y `combine()`.
 */
template <typename T, typename HasherType = StdMerkleHasher<T>>
struct MerkleTreeTraits {
    using value_type = T;
    using Hasher = HasherType;
    using Digest = typename Hasher::Digest;
    using Node = MerkleNode<Digest>;
    using ForwardIterator = BinaryTreeIterator<const Node, TreeOrder::PostOrder>;
    using BackwardIterator = BinaryTreeIterator<const Node, TreeOrder::PostOrder, true>;
};

#endif // __CONTAINERS_TRAITS_H__
