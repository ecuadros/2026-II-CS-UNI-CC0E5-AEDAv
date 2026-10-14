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

template <typename T>
class LinkedListNode : public GeneralNode<T>, public NextLink<LinkedListNode<T>> {
    using Node = LinkedListNode<T>;
public:
    LinkedListNode() : GeneralNode<T>(T{}, Ref{}) {}
    LinkedListNode(const T& value, Ref ref, Node* pNext) : GeneralNode<T>(value, ref) {
        this->m_pNext = pNext;
    }
};

template <typename T>
class DoubleLinkedNode : public LinkedListNode<T>,
                         public NextLink<DoubleLinkedNode<T>>,
                         public PrevLink<DoubleLinkedNode<T>> {
  using Node = DoubleLinkedNode<T>;

public:
  using NextLink<Node>::m_pNext;

  DoubleLinkedNode() = default;
  DoubleLinkedNode(const T &value, Ref ref, Node *pNext, Node *pPrev = nullptr)
      : LinkedListNode<T>(value, ref, nullptr) {
    this->m_pNext = pNext;
    this->m_pPrev = pPrev;
  }
};

template <typename T, typename NodeType = LinkedListNode<T>>
using LinkedListForwardIterator = ListIterator<NodeType>;

template <typename T>
using DoubleLinkedListBackwardIterator = ListIterator<DoubleLinkedNode<T>, true>;

#endif // __LIST_NODES_H__
