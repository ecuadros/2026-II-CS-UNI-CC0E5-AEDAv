#ifndef __DOUBLE_LINKED_LIST__
#define __DOUBLE_LINKED_LIST__

#include "linkedlist.h"

template <typename Traits> class DoubleLinkedList : public LinkedList<Traits> {
public:
  using Parent = LinkedList<Traits>;
  using value_type = typename Traits::value_type;
  using Node = typename Traits::Node;
  using NodePtr = Node *;
  using BackwardIterator = typename Traits::BackwardIterator;

  DoubleLinkedList() = default;
  DoubleLinkedList(const DoubleLinkedList &other) : Parent(other) {
    corregirPunteros();
  }
  DoubleLinkedList(std::initializer_list<std::pair<value_type, Ref>> values)
      : Parent(values) {
    corregirPunteros();
  }

protected:
  void corregirPunteros() override {
    NodePtr previous = nullptr;
    NodePtr current = this->m_pRoot;

    while (current != nullptr) {
      current->m_pPrev = previous;
      previous = current;
      current = current->m_pNext;

      if (current == this->m_pRoot)
        break;
    }

    this->m_pTail = previous;

    if (current == this->m_pRoot && current != nullptr)
      this->m_pRoot->m_pPrev = this->m_pTail;
  }

  void completarInsercion(NodePtr new_node, NodePtr previous) override {
    new_node->m_pPrev = previous;
  }

public:
  BackwardIterator rbegin() const { return BackwardIterator(this->m_pTail); }
  BackwardIterator rend() const { return BackwardIterator(nullptr); }
};

#endif // DOUBLE_LINKED_LIST
