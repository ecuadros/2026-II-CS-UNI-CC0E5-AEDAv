#ifndef LISTITERATOR_H
#define LISTITERATOR_H

#include "GeneralIterator.h"

/**
 * Iterador de listas enlazadas.
 * @tparam Node Tipo de nodo.
 * @tparam Reverse Si es `true`, avanza por `m_pPrev` en vez de `m_pNext`.
 */
template <typename Node, bool Reverse = false>
class ListIterator : public GeneralIterator<ListIterator<Node, Reverse>, Node> {
  using Parent = GeneralIterator<ListIterator<Node, Reverse>, Node>;

public:
  /** Crea el iterador sobre el nodo @p ptr. */
  explicit ListIterator(Node *ptr) : Parent(ptr) {}

  /** Avanza al nodo siguiente. Si vuelve al nodo inicial (lista circular) pasa a `nullptr`, que equivale a `end()`. */
  ListIterator &operator++() {
    if constexpr (Reverse)
      Parent::m_ptr = Parent::m_ptr->m_pPrev;
    else
      Parent::m_ptr = Parent::m_ptr->m_pNext;
    if (Parent::m_ptr == Parent::m_pStart)
      Parent::m_ptr = nullptr;
    return *this;
  }
};

#endif // LISTITERATOR_H
