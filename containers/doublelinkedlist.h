#ifndef __DOUBLE_LINKED_LIST__
#define __DOUBLE_LINKED_LIST__

#include "GeneralIterator.h"
#include "GeneralNode.h"
#include "linkedlist.h"
#include <cstddef>
#include <mutex>

template <typename T> class DoubleLinkedNode : public LinkedListNode<T> {
  using Node = DoubleLinkedNode<T>;

public:
  Node *m_pPrev = nullptr; // puntero al nodo anterior, para acceso directo
  DoubleLinkedNode() = default;
  DoubleLinkedNode(const T &value, Ref ref, Node *pNext, Node *pPrev = nullptr)
      : LinkedListNode<T>(value, ref, pNext), m_pPrev(pPrev) {}
};

template <typename T>
class DoubleLinkedListBackwardIterator
    : public GeneralIterator<DoubleLinkedListBackwardIterator<T>,
                             DoubleLinkedNode<T>> {
public:
  using value_type = DoubleLinkedNode<T>;
  using MySelf = DoubleLinkedListBackwardIterator<T>;
  using Parent = GeneralIterator<MySelf, value_type>;

private:
  value_type *m_start;

public:
  DoubleLinkedListBackwardIterator(value_type *ptr)
      : Parent(ptr), m_start(ptr) {}
  DoubleLinkedListBackwardIterator &operator++() {
    Parent::m_ptr = Parent::m_ptr->m_pPrev;
    if (Parent::m_ptr == m_start)
      Parent::m_ptr = nullptr;
    return *this;
  }
};

template <typename Traits> class DoubleLinkedList : public LinkedList<Traits> {
public:
  using value_type = typename Traits::value_type;
  using Node = typename Traits::Node;
  using NodePtr = Node *;
  using ForwardIterator = typename Traits::ForwardIterator;
  using BackwardIterator = typename Traits::BackwardIterator;

public:
  DoubleLinkedList() = default;

  DoubleLinkedList(const DoubleLinkedList &other) : LinkedList<Traits>(other) {
    NodePtr anterior = nullptr;
    NodePtr actual = this->m_pRoot;

    while (actual != nullptr) {
      actual->m_pPrev = anterior;
      anterior = actual;
      actual = static_cast<NodePtr>(actual->m_pNext);
      if (actual == this->m_pRoot)
        break; // lista circular
    }
    this->m_pTail = anterior;
  }

  void insert(const typename Traits::value_type &value, Ref ref) override {
    std::scoped_lock lock(this->m_mutex);
    this->internalInsert(value, ref, this->m_pRoot);
    NodePtr anterior = nullptr;
    NodePtr actual = this->m_pRoot;

    while (actual != nullptr) {
      actual->m_pPrev = anterior;
      anterior = actual;
      actual = static_cast<NodePtr>(actual->m_pNext);
      if (actual == this->m_pRoot)
        break; // lista circular
    }
    this->m_pTail = anterior;
  }

  DoubleLinkedList &operator=(const DoubleLinkedList &other) {
    if (this != &other) {
      LinkedList<Traits>::operator=(other);
      NodePtr anterior = nullptr;
      NodePtr actual = this->m_pRoot;

      while (actual != nullptr) {
        actual->m_pPrev = anterior;
        anterior = actual;
        actual = static_cast<NodePtr>(actual->m_pNext);
        if (actual == this->m_pRoot)
          break; // lista circular
      }
      this->m_pTail = anterior;
    }
    return *this;
  }

  void push_back(const value_type &value, Ref ref) override {
    std::scoped_lock lock(this->m_mutex);

    NodePtr node = new Node(value, ref, nullptr);
    node->m_pPrev = this->m_pTail;

    if (this->m_pTail == nullptr)
      this->m_pRoot = node;
    else
      this->m_pTail->m_pNext = node;

    this->m_pTail = node;
  }

  BackwardIterator rbegin() const { return BackwardIterator(this->m_pTail); }
  BackwardIterator rend() const { return BackwardIterator(nullptr); }

  template <typename Func, typename... Args>
  decltype(auto) rcall(Func func, Args &&...args) {
    std::lock_guard<std::mutex> lock(this->m_mutex);
    if constexpr (std::is_void_v<std::invoke_result_t<Func, Node &, Args...>>)
      ::call(rbegin(), rend(), std::forward<Func>(func),
             std::forward<Args>(args)...);
    else
      return ::call(rbegin(), rend(), std::forward<Func>(func),
                    std::forward<Args>(args)...);
  }
};

#endif // DOUBLE_LINKED_LIST
