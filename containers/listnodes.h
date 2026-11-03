#ifndef __LIST_NODES_H__
#define __LIST_NODES_H__

#include "GeneralNode.h"
#include "listiterator.h"

template <class Node>
struct NextLink {
  Node* m_pNext = nullptr;
};

template <class Node>
struct PrevLink {
  Node* m_pPrev = nullptr;
};

template <typename T, typename Derived = void>
class LinkedListNode : public GeneralNode<T>,
                       public NextLink<NodeSelf<LinkedListNode<T>, Derived>> {
    using Node = NodeSelf<LinkedListNode<T>, Derived>;
public:
    LinkedListNode() : GeneralNode<T>(T{}, Ref{}) {}
    LinkedListNode(const T& value, Ref ref, Node* pNext) : GeneralNode<T>(value, ref) {
        this->m_pNext = pNext;
    }
};

template <typename T>
class DoubleLinkedNode : public LinkedListNode<T, DoubleLinkedNode<T>>,
                         public PrevLink<DoubleLinkedNode<T>> {
  using Node = DoubleLinkedNode<T>;
  using Parent = LinkedListNode<T, Node>;

public:
  DoubleLinkedNode() = default;
  DoubleLinkedNode(const T &value, Ref ref, Node *pNext, Node *pPrev = nullptr)
      : Parent(value, ref, pNext) {
    this->m_pPrev = pPrev;
  }
};

template <typename T, typename NodeType = LinkedListNode<T>>
using LinkedListForwardIterator = ListIterator<NodeType>;

template <typename T>
using DoubleLinkedListBackwardIterator = ListIterator<DoubleLinkedNode<T>, true>;

#endif // __LIST_NODES_H__
