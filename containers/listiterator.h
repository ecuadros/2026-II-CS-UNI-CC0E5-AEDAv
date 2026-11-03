#ifndef LISTITERATOR_H
#define LISTITERATOR_H

#include "GeneralIterator.h"

template <typename Node, bool Reverse = false>
class ListIterator : public GeneralIterator<ListIterator<Node, Reverse>, Node> {
  using Parent = GeneralIterator<ListIterator<Node, Reverse>, Node>;

public:
  explicit ListIterator(Node *ptr) : Parent(ptr) {}

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
