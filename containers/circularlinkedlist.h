#ifndef __CIRCULAR_LINKED_LIST__
#define __CIRCULAR_LINKED_LIST__

// Clase para una lista cirtular enlazada

#include "linkedlist.h"
#include <mutex>

template <typename Traits>
class CircularLinkedList : public LinkedList<Traits> {
public:
  using value_type = typename Traits::value_type;
  using Node = typename Traits::Node;
  using NodePtr = Node *;
  using ForwardIterator = typename Traits::ForwardIterator;
  using Compare = typename Traits::Compare;
  using Delim = typename Node::Delim;

private:
  NodePtr GetRoot() const { return this->m_pRoot; }
  NodePtr GetTail() const { return this->m_pTail; }

public:
  CircularLinkedList() = default;
  void clear() override {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    if (!this->m_pRoot)
      return;
    NodePtr current = this->m_pRoot;
    do {
      NodePtr next = current->m_pNext;
      delete current;
      current = next;
    } while (current != this->m_pRoot);
    this->m_pRoot = nullptr;
    this->m_pTail = nullptr;
  }

  void push_back(const value_type &value, Ref ref) override {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    NodePtr new_node = new Node(value, ref, nullptr);
    if (!this->m_pRoot) {
      this->m_pRoot = new_node;
      this->m_pTail = new_node;
      this->m_pTail->m_pNext = this->m_pRoot;
    } else {
      this->m_pTail->m_pNext = new_node;
      this->m_pTail = new_node;
      this->m_pTail->m_pNext = this->m_pRoot;
    }
  }

  ~CircularLinkedList() override { clear(); }

  void insert(const value_type &value, Ref ref) override {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    this->internalInsert(value, ref, this->m_pRoot);
    this->m_pTail->m_pNext = this->m_pRoot;
  }
};

#endif // !__CIRCULAR_LINKED_LIST__
