#ifndef __DOUBLE_LINKED_LIST__
#define __DOUBLE_LINKED_LIST__

#include "GeneralIterator.h"
#include "GeneralNode.h"
#include "linkedlist.h"

template <typename T>
class DoubleLinkedNode : public GeneralNode<T>,
                         public NextLink<DoubleLinkedNode<T>> {
  using Node = DoubleLinkedNode<T>;

public:
  Node *m_pPrev = nullptr;
  DoubleLinkedNode() : GeneralNode<T>(T{}, Ref{}) {};
  DoubleLinkedNode(const T &value, Ref ref, Node *pNext, Node *pPrev = nullptr)
      : GeneralNode<T>(value, ref), m_pPrev(pPrev) {
    this->m_pNext = pNext;
  }
};

template <typename T>
using DoubleLinkedListBackwardIterator =
    ListIterator<DoubleLinkedNode<T>, true>;

template <typename Traits> class DoubleLinkedList : public LinkedList<Traits> {
public:
  using Node = typename Traits::Node;
  using NodePtr = Node *;
  using BackwardIterator = typename Traits::BackwardIterator;

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
