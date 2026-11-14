#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include "listnodes.h"
#include "traversalops.h"
#include <initializer_list>
#include <mutex>
#include <utility>

/**
 * Lista enlazada configurable en tiempo de compilación.
 * @tparam Traits Tipos de valor, nodo, iteradores y comparador.
 * @tparam Double Si es `true`, cada nodo enlaza también con el anterior.
 * @tparam Circular Si es `true`, el último nodo enlaza con el primero.
 * @tparam Ops Mixins de recorrido, impresión y lectura.
 */
template <typename Traits, bool Double = false, bool Circular = false,
          typename Ops = FullTraversalOps>
class LinkedList : public Ops {
public:
  using value_type = typename Traits::value_type;
  using Node = typename Traits::Node;
  using NodePtr = Node *;
  using ForwardIterator = typename Traits::ForwardIterator;
  using Compare = typename Traits::Compare;
  static constexpr bool is_circular = Circular;

protected:
  NodePtr m_pRoot = nullptr; // puntero al primer nodo de la lista enlazada
  NodePtr m_pTail = nullptr; // puntero al último nodo de la lista enlazada

  Compare m_comp; // comparador para ordenar los nodos de la lista

  /** Enlaza @p node detrás de @p prev, o al inicio si @p prev es nulo. Mantiene la cola y los enlaces dobles y circulares. */
  void link_after(NodePtr prev, NodePtr node) {
    node->m_pNext = prev ? prev->m_pNext : m_pRoot;
    if (prev)
      prev->m_pNext = node;
    else
      m_pRoot = node;
    if (prev == m_pTail)
      m_pTail = node;
    if constexpr (Circular)
      m_pTail->m_pNext = m_pRoot;
    if constexpr (Double) {
      node->m_pPrev = prev;
      if (node->m_pNext)
        node->m_pNext->m_pPrev = node;
      if constexpr (Circular)
        m_pRoot->m_pPrev = m_pTail;
    }
  }

public:
  /** Crea una lista vacía. */
  LinkedList() = default;
  /** Crea una lista con los pares (valor, ref) dados, en ese orden. */
  LinkedList(std::initializer_list<std::pair<value_type, Ref>> values) {
    fill_from(*this, values);
  }

  /** Copia los nodos de @p other conservando su orden. */
  LinkedList(const LinkedList &other) {
    std::lock_guard lock(other.m_mutex);
    try {
      for (auto it = other.begin(); it != other.end(); ++it)
        link_after(m_pTail, new Node(it->getValue(), it->getRef(), nullptr));
    } catch (...) {
      clear();
      throw;
    }
  }

  /** Mueve el contenido de @p other, que queda vacía. */
  LinkedList(LinkedList &&other) noexcept { swap(other); }

  /** Asigna por copia o por movimiento (copy-and-swap). */
  LinkedList &operator=(LinkedList other) noexcept {
    swap(other);
    return *this;
  }

  /** Intercambia los nodos con @p other. */
  void swap(LinkedList &other) noexcept {
    if (this == &other)
      return;
    std::scoped_lock lock(this->m_mutex, other.m_mutex);
    std::swap(m_pRoot, other.m_pRoot);
    std::swap(m_pTail, other.m_pTail);
  }

  /** Libera todos los nodos. */
  ~LinkedList() { clear(); }

  /** Elimina todos los nodos. */
  void clear() {
    std::lock_guard lock(this->m_mutex);
    if constexpr (Circular) {
      if (m_pTail)
        m_pTail->m_pNext = nullptr;
    }
    for (NodePtr current = m_pRoot; current;)
      delete std::exchange(current, current->m_pNext);
    m_pRoot = m_pTail = nullptr;
  }

  /** Añade un nodo al final, sin respetar el orden. */
  void push_back(const value_type &value, Ref ref) {
    std::lock_guard lock(this->m_mutex);
    link_after(m_pTail, new Node(value, ref, nullptr));
  }

  /** Inserta el valor en la posición que le corresponde según el comparador; entre equivalentes queda el último. */
  void insert(const value_type &value, Ref ref) {
    std::lock_guard lock(this->m_mutex);
    NodePtr prev = nullptr;
    for (NodePtr cur = m_pRoot;
         prev != m_pTail && !m_comp(value, cur->getValue()); cur = cur->m_pNext)
      prev = cur;
    link_after(prev, new Node(value, ref, nullptr));
  }

  /** Indica si la lista no tiene nodos. */
  bool empty() const {
    std::lock_guard lock(this->m_mutex);
    return m_pRoot == nullptr;
  }

  /** Iterador al primer nodo. */
  ForwardIterator begin() const { return ForwardIterator(m_pRoot); }
  /** Iterador de fin. */
  ForwardIterator end() const { return ForwardIterator(nullptr); }

  /** Iterador inverso al último nodo; solo existe en las listas dobles. */
  auto rbegin() const
    requires Double
  {
    return typename Traits::BackwardIterator(m_pTail);
  }
  /** Iterador inverso de fin; solo existe en las listas dobles. */
  auto rend() const
    requires Double
  {
    return typename Traits::BackwardIterator(nullptr);
  }
};

/** Lista doblemente enlazada. */
template <typename Traits>
using DoubleLinkedList = LinkedList<Traits, true, false>;

/** Lista circular simplemente enlazada. */
template <typename Traits>
using CircularLinkedList = LinkedList<Traits, false, true>;

/** Lista circular doblemente enlazada. */
template <typename Traits>
using CircularDoubleLinkedList = LinkedList<Traits, true, true>;

#endif // __LINKEDLIST_H__
