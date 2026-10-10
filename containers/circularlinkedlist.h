#ifndef __CIRCULAR_LINKED_LIST__
#define __CIRCULAR_LINKED_LIST__

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

protected:
  void corregirPunteros() override { this->m_pTail->m_pNext = this->m_pRoot; }

private:
  NodePtr GetRoot() const { return this->m_pRoot; }
  NodePtr GetTail() const { return this->m_pTail; }

public:
  CircularLinkedList() = default;
  CircularLinkedList(const CircularLinkedList &other)
      : LinkedList<Traits>(other) {}

  CircularLinkedList &operator=(const CircularLinkedList &other) {
    LinkedList<Traits>::operator=(other);
    return *this;
  }

  void clear() override {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    if (!this->m_pRoot)
      return;
    this->m_pTail->m_pNext = nullptr;
    NodePtr current = this->m_pRoot;
    while (current != nullptr) {
      NodePtr next = current->m_pNext;
      delete current;
      current = next;
    }
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
    LinkedList<Traits>::insert(value, ref);
  }
};

#endif // !__CIRCULAR_LINKED_LIST__
