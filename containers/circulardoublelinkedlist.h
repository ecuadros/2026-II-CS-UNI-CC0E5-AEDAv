#ifndef __CIRCULAR_DOUBLE_LINKED_LIST__
#define __CIRCULAR_DOUBLE_LINKED_LIST__

#include "doublelinkedlist.h"
template <typename Traits>
class CircularDoubleLinkedList : public DoubleLinkedList<Traits> {

public:
  using Node = typename Traits::Node;
  using NodePtr = Node *;

protected:
  void corregirPunteros() override {
    if (this->m_pTail) {
      this->m_pTail->m_pNext = this->m_pRoot;
      this->m_pRoot->m_pPrev = this->m_pTail;
    }
    DoubleLinkedList<Traits>::corregirPunteros();
  }

  void completarInsercion(NodePtr new_node, NodePtr previous) override {
    new_node->m_pPrev = previous ? previous : new_node;
    new_node->m_pNext = this->m_pRoot;
    this->m_pRoot->m_pPrev = new_node;
  }

};

#endif
