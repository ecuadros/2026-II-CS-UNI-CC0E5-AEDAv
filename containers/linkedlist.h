#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include <initializer_list>
#include <mutex>
#include <utility>
#include "listnodes.h"
#include "traversalops.h"
#include "container_stream.h"

template <typename Traits, bool Double = false, bool Circular = false>
class LinkedList : public FullTraversalOps<LinkedList<Traits, Double, Circular>> {
    friend class CallOp<LinkedList>;
    friend class FirstThatOp<LinkedList>;
    friend class ApplyFunctionOp<LinkedList>;
public:
    using container_stream_tag = void;
    using traits_type       = Traits;
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node *;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using Compare           = typename Traits::Compare;
protected:
    NodePtr m_pRoot = nullptr; // puntero al primer nodo de la lista enlazada
    NodePtr m_pTail = nullptr; // puntero al último nodo de la lista enlazada

    Compare m_comp; // comparador para ordenar los nodos de la lista
    mutable std::mutex m_mutex; // mutex para sincronización

    void internalInsert(const value_type& value, Ref ref, NodePtr& rParent);
    void corregirPunteros() {
        if constexpr (Circular) {
            if (m_pTail)
                m_pTail->m_pNext = m_pRoot;
        }
        if constexpr (Double) {
            NodePtr previous = nullptr;
            NodePtr current = m_pRoot;
            while (current != nullptr) {
                current->m_pPrev = previous;
                previous = current;
                current = current->m_pNext;
                if (current == m_pRoot)
                    break;
            }
            m_pTail = previous;
            if (Circular && m_pRoot)
                m_pRoot->m_pPrev = m_pTail;
        }
    }

    void completarInsercion(NodePtr new_node, NodePtr previous) {
        if constexpr (Double)
            new_node->m_pPrev = previous;
        if constexpr (Circular) {
            new_node->m_pNext = m_pRoot;
            if constexpr (Double)
                m_pRoot->m_pPrev = new_node;
        }
    }

public:
    LinkedList() = default;
    LinkedList(const LinkedList& another){ *this = another; }
    LinkedList& operator=(const LinkedList& another);
    LinkedList(std::initializer_list<std::pair<value_type, Ref>> values) {
        for (const auto &v : values)
            push_back(v.first, v.second);
    }

    void clear();
    ~LinkedList(){ clear(); }

    void push_back(const value_type& value, Ref ref);

    bool empty() const { std::scoped_lock lock(m_mutex); return m_pRoot == nullptr; }

    void insert(const value_type& value, Ref ref) {
        std::scoped_lock lock(m_mutex);
        internalInsert(value, ref, m_pRoot);
        corregirPunteros();
    }

    std::ostream& write(std::ostream& os) const {
        std::lock_guard lock(m_mutex);
        return container_stream::write(os, begin(), end());
    }

    std::istream& read(std::istream& is) {
        std::vector<std::pair<value_type, Ref>> values;
        if (!container_stream::read(is, values)) return is;
        LinkedList parsed;
        for (const auto& [value, ref] : values) parsed.push_back(value, ref);
        std::scoped_lock lock(m_mutex, parsed.m_mutex);
        std::swap(m_pRoot, parsed.m_pRoot);
        std::swap(m_pTail, parsed.m_pTail);
        return is;
    }

    // Iterators
    ForwardIterator begin() const { return ForwardIterator(m_pRoot); }
    ForwardIterator end() const { return ForwardIterator(nullptr); }

    auto rbegin() const requires Double { return typename Traits::BackwardIterator(m_pTail); }
    auto rend() const requires Double { return typename Traits::BackwardIterator(nullptr); }

};


template <typename Traits, bool Double, bool Circular>
LinkedList<Traits, Double, Circular>& LinkedList<Traits, Double, Circular>::operator=(const LinkedList<Traits, Double, Circular>& other){
    if (this == &other) return *this;
    LinkedList copy;
    {
        std::lock_guard lock(other.m_mutex);
        for (auto it = other.begin(); it != other.end(); ++it)
            copy.push_back((*it).getValue(), (*it).getRef());
    }
    {
        std::scoped_lock lock(m_mutex, copy.m_mutex);
        std::swap(m_pRoot, copy.m_pRoot);
        std::swap(m_pTail, copy.m_pTail);
    }
    return *this;
}

template <typename Traits, bool Double, bool Circular>
void LinkedList<Traits, Double, Circular>::clear(){
    std::scoped_lock lock(m_mutex);
    if constexpr (Circular) {
        if (m_pTail) m_pTail->m_pNext = nullptr;
    }

    NodePtr current = m_pRoot;
    while(current != nullptr){
        NodePtr next = current->m_pNext;
        delete current;
        current = next;
    }
    m_pRoot = m_pTail = nullptr;
}

template <typename Traits, bool Double, bool Circular>
void LinkedList<Traits, Double, Circular>::push_back(const value_type& value, Ref ref){
    std::scoped_lock lock(m_mutex);
    NodePtr previous = m_pTail;
    NodePtr new_node = new Node(value, ref, nullptr);

    if (previous) previous->m_pNext = new_node;
    else m_pRoot = new_node;
    m_pTail = new_node;
    completarInsercion(new_node, previous);
}

/*
internalInsert(...) recorre todos los nodos hasta llegar al final, y en la condición de parada
si el nodo padre es nullptr, crea uno nuevo con el valor y retorna
insert(...) empieza desde el nodo raiz
*/
template <typename Traits, bool Double, bool Circular>
void LinkedList<Traits, Double, Circular>::internalInsert(const value_type& value, Ref ref, NodePtr& rParent) {
    if (rParent == nullptr || m_comp(value, rParent->getValue())) {
        NodePtr temp = rParent;
        rParent = new Node(value, ref, rParent);
        if (temp == nullptr) {
            m_pTail = rParent;
        }
        return;
    }

    if (rParent == m_pTail) {
        NodePtr new_node = new Node(value, ref, nullptr);
        rParent->m_pNext = new_node;
        m_pTail = new_node;
        return;
    }

    NodePtr next = rParent->m_pNext;
    internalInsert(value, ref, next);
    rParent->m_pNext = next;
}

template <typename Traits>
using DoubleLinkedList = LinkedList<Traits, true, false>;

template <typename Traits>
using CircularLinkedList = LinkedList<Traits, false, true>;

template <typename Traits>
using CircularDoubleLinkedList = LinkedList<Traits, true, true>;

#endif // __LINKEDLIST_H__
