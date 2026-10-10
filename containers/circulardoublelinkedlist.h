#ifndef __CIRCULAR_DOUBLE_LINKED_LIST__
#define __CIRCULAR_DOUBLE_LINKED_LIST__

#include "doublelinkedlist.h"
template <typename Traits>
class CircularDoubleLinkedList : public DoubleLinkedList<Traits> {

public:
  using value_type = typename Traits::value_type;
  using Node = typename Traits::Node;
  using NodePtr = Node *;
  using ForwardIterator = typename Traits::ForwardIterator;
  using BackwardIterator = typename Traits::BackwardIterator;

public:
  CircularDoubleLinkedList() = default;

  CircularDoubleLinkedList(const CircularDoubleLinkedList &other)
      : DoubleLinkedList<Traits>(other) {
    if (this->m_pRoot != nullptr)
      this->m_pRoot->m_pPrev = this->m_pTail;
  }

  CircularDoubleLinkedList &operator=(const CircularDoubleLinkedList &other) {
    if (this == &other)
      return *this;
    DoubleLinkedList<Traits>::operator=(other);
    if (this->m_pRoot == nullptr)
      return *this;

    NodePtr previous = this->m_pTail;
    NodePtr current = this->m_pRoot;
    do {
      current->m_pPrev = previous;
      previous = current;
      current = static_cast<NodePtr>(current->m_pNext);
    } while (current != this->m_pRoot);
    return *this;
  }

  void clear() override {
    std::scoped_lock lock(this->m_mutex);
    if (this->m_pRoot == nullptr)
      return;

    this->m_pTail->m_pNext = nullptr;
    NodePtr current = this->m_pRoot;
    while (current != nullptr) {
      NodePtr next = static_cast<NodePtr>(current->m_pNext);
      delete current;
      current = next;
    }
    this->m_pRoot = nullptr;
    this->m_pTail = nullptr;
  }

  ~CircularDoubleLinkedList() override { clear(); }

  void push_back(const value_type &value, Ref ref) override {
    std::scoped_lock lock(this->m_mutex);
    NodePtr node = new Node(value, ref, this->m_pRoot, this->m_pTail);

    if (this->m_pRoot == nullptr) {
      this->m_pRoot = node;
      this->m_pTail = node;
      node->m_pNext = node;
      node->m_pPrev = node;
    } else {
      this->m_pTail->m_pNext = node;
      this->m_pRoot->m_pPrev = node;
      node->m_pPrev = this->m_pTail;
      node->m_pNext = this->m_pRoot;
      this->m_pTail = node;
    }
  }

  void insert(const value_type &value, Ref ref) override {
    std::scoped_lock lock(this->m_mutex);
    this->internalInsert(value, ref, this->m_pRoot);
    this->m_pTail->m_pNext = this->m_pRoot;

    NodePtr previous = this->m_pTail;
    NodePtr current = this->m_pRoot;
    do {
      current->m_pPrev = previous;
      previous = current;
      current = static_cast<NodePtr>(current->m_pNext);
    } while (current != this->m_pRoot);
  }
};

#endif
