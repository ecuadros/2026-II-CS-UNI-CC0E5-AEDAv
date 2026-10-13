#ifndef __CIRCULAR_DOUBLE_LINKED_LIST__
#define __CIRCULAR_DOUBLE_LINKED_LIST__

#include "doublelinkedlist.h"
template <typename Traits>
class CircularDoubleLinkedList : public DoubleLinkedList<Traits> {

public:
  using Parent = DoubleLinkedList<Traits>;
  using value_type = typename Traits::value_type;
  using Node = typename Traits::Node;
  using NodePtr = Node *;

  CircularDoubleLinkedList() = default;
  CircularDoubleLinkedList(const CircularDoubleLinkedList &other)
      : Parent(other) {
    corregirPunteros();
  }
  CircularDoubleLinkedList(
      std::initializer_list<std::pair<value_type, Ref>> values)
      : Parent(values) {
    corregirPunteros();
  }

protected:
  void corregirPunteros() override {
    if (this->m_pTail) {
      this->m_pTail->m_pNext = this->m_pRoot;
    }
    Parent::corregirPunteros();
  }

  void completarInsercion(NodePtr new_node, NodePtr previous) override {
    Parent::completarInsercion(new_node, previous);
    new_node->m_pNext = this->m_pRoot;
    this->m_pRoot->m_pPrev = new_node;
  }
};

#endif
