#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include <mutex>
#include "listnodes.h"
#include "traversalops.h"

template <typename Traits, bool Double = false, bool Circular = false>
class LinkedList : public TraversalOps<LinkedList<Traits, Double, Circular>> {
    friend class TraversalOps<LinkedList>;
public:
    using traits_type       = Traits;
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node *;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using Compare           = typename Traits::Compare;
    using Delim             = typename Node::Delim;
protected:
    NodePtr m_pRoot = nullptr; // puntero al primer nodo de la lista enlazada
    NodePtr m_pTail = nullptr; // puntero al último nodo de la lista enlazada
    
    Compare m_comp; // comparador para ordenar los nodos de la lista
    mutable std::mutex m_mutex; // mutex para sincronización

    NodePtr GetRoot() const { return m_pRoot; }
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
    LinkedList(const LinkedList& another){ *this = another; } // copia profunda de la lista enlazada
    LinkedList& operator=(const LinkedList& another); // no se permite asignacion
    LinkedList(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            push_back(v.first, v.second);
    }

    void clear();
    ~LinkedList(){ clear(); };

    void push_back(const value_type& value, Ref ref);

    bool empty() const { return m_pRoot == nullptr; }

    void insert(const value_type& value, Ref ref) {
        scoped_lock lock(m_mutex);
        internalInsert(value, ref, m_pRoot);
        corregirPunteros();
    }

    std::ostream& write(std::ostream& os) { return os << *this; }
    std::istream& read(std::istream& is) { return is >> *this; }

    friend std::ostream& operator <<(std::ostream& os, const LinkedList& list) {
        lock_guard lock(list.m_mutex);
        auto first = true;
        os << "[";
        for (auto it = list.begin(); it != list.end(); ++it){
            if (!first)
                os << ",";
            os << *it;
            first = false;
        }
        return os << "]";
    }
    
    friend std::istream &operator >>(std::istream &is, LinkedList &list) {
        Delim d;
        Node node;
        list.clear();

        is >> d;
        if (is >> d && d != ']'){
            is.unget();
            while(is >> node >> d){
                list.push_back(node.getValue(), node.getRef());
                if (d == ']'){
                    break;
                }
            }    
        }

        return is; 
    }
    
    // Iterators
    // ForwardIterator begin() { return ForwardIterator(m_pRoot); }
    // ForwardIterator end() { return ForwardIterator(nullptr); }
    ForwardIterator begin() const { return ForwardIterator(m_pRoot); }
    ForwardIterator end() const { return ForwardIterator(nullptr); }

    auto rbegin() const requires Double { return typename Traits::BackwardIterator(m_pTail); }
    auto rend() const requires Double { return typename Traits::BackwardIterator(nullptr); }

};


template <typename Traits, bool Double, bool Circular>
LinkedList<Traits, Double, Circular>& LinkedList<Traits, Double, Circular>::operator=(const LinkedList<Traits, Double, Circular>& other){ 
    if(this == &other)
        return *this;
    clear();
    if(!other.m_pRoot)
        return *this;
    std::lock_guard<std::mutex> lock(other.m_mutex);

    m_pRoot = new Node(other.GetRoot()->getValue(), other.GetRoot()->getRef(), nullptr);

    NodePtr next = other.m_pRoot->m_pNext;
    NodePtr curr = m_pRoot;

    while(next != nullptr && next != other.m_pRoot){
        curr->m_pNext = new Node(next->getValue(), next->getRef(), nullptr);
        curr = curr->m_pNext;
        next = next->m_pNext;
    }
    m_pTail = curr;
    corregirPunteros();
    return *this;
}

template <typename Traits, bool Double, bool Circular>
void LinkedList<Traits, Double, Circular>::clear(){
    scoped_lock lock(m_mutex);
    if(m_pTail && m_pTail->m_pNext == m_pRoot)
        m_pTail->m_pNext = nullptr;

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
    scoped_lock lock(m_mutex);
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
