#ifndef __LIST_NODES_H__
#define __LIST_NODES_H__

#include "GeneralNode.h"
#include "listiterator.h"

/** Enlace al nodo siguiente. */
template <class Node>
struct NextLink {
  Node* m_pNext = nullptr;
};

/** Enlace al nodo anterior. */
template <class Node>
struct PrevLink {
  Node* m_pPrev = nullptr;
};

/**
 * Nodo de lista simplemente enlazada.
 * @tparam T Tipo del valor.
 * @tparam Derived Nodo derivado al que apuntan los enlaces, o `void` si es este mismo.
 */
template <typename T, typename Derived = void>
class LinkedListNode : public GeneralNode<T>,
                       public NextLink<NodeSelf<LinkedListNode<T>, Derived>> {
    using Node = NodeSelf<LinkedListNode<T>, Derived>;
public:
    /** Crea un nodo vacío sin siguiente. */
    LinkedListNode() : GeneralNode<T>(T{}, Ref{}) {}
    /** Crea un nodo con su valor, referencia y nodo siguiente. */
    LinkedListNode(const T& value, Ref ref, Node* pNext) : GeneralNode<T>(value, ref) {
        this->m_pNext = pNext;
    }
};

/**
 * Nodo de lista doblemente enlazada: añade el enlace al nodo anterior.
 * @tparam T Tipo del valor.
 */
template <typename T>
class DoubleLinkedNode : public LinkedListNode<T, DoubleLinkedNode<T>>,
                         public PrevLink<DoubleLinkedNode<T>> {
  using Node = DoubleLinkedNode<T>;
  using Parent = LinkedListNode<T, Node>;

public:
  /** Crea un nodo vacío sin enlaces. */
  DoubleLinkedNode() = default;
  /** Crea un nodo con su valor, referencia y enlaces siguiente y anterior. */
  DoubleLinkedNode(const T &value, Ref ref, Node *pNext, Node *pPrev = nullptr)
      : Parent(value, ref, pNext) {
    this->m_pPrev = pPrev;
  }
};

/** Iterador hacia adelante de las listas enlazadas. */
template <typename T, typename NodeType = LinkedListNode<T>>
using LinkedListForwardIterator = ListIterator<NodeType>;

/** Iterador hacia atrás de las listas doblemente enlazadas. */
template <typename T>
using DoubleLinkedListBackwardIterator = ListIterator<DoubleLinkedNode<T>, true>;

#endif // __LIST_NODES_H__
