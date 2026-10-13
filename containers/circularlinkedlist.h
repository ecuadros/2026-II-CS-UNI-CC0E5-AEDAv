#ifndef __CIRCULAR_LINKED_LIST__
#define __CIRCULAR_LINKED_LIST__

#include "linkedlist.h"

template <typename Traits>
class CircularLinkedList : public LinkedList<Traits> {
public:
  using Node = typename Traits::Node;
  using NodePtr = Node *;

protected:
  void corregirPunteros() override {
    if (this->m_pTail) {
      this->m_pTail->m_pNext = this->m_pRoot;
    }
  }

  void completarInsercion(NodePtr new_node, NodePtr) override {
    new_node->m_pNext = this->m_pRoot;
  }

};

#endif // !__CIRCULAR_LINKED_LIST__
