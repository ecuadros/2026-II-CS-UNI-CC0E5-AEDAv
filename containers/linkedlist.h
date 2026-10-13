#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include <mutex>
#include "listnodes.h"
#include "traversalops.h"

template <typename Traits>
class LinkedList : public TraversalOps<LinkedList<Traits>> {
    friend class TraversalOps<LinkedList<Traits>>;
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
    virtual void corregirPunteros() {};
    virtual void completarInsercion(NodePtr, NodePtr) {};

public:
    LinkedList() = default;
    LinkedList(const LinkedList& another){ *this = another; } // copia profunda de la lista enlazada
    LinkedList& operator=(const LinkedList& another); // no se permite asignacion
    LinkedList(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            push_back(v.first, v.second);
    }

    virtual void clear();
    virtual ~LinkedList(){ clear(); };

    virtual void push_back(const value_type& value, Ref ref);

    bool empty() const { return m_pRoot == nullptr; }

    virtual void insert(const value_type& value, Ref ref) {
        scoped_lock lock(m_mutex);
        internalInsert(value, ref, m_pRoot);
        corregirPunteros();
    }

    std::ostream& write(std::ostream& os) { return os << *this; }
    std::istream& read(std::istream& is) { return is >> *this; }

    friend std::ostream& operator <<(std::ostream& os, const LinkedList<Traits>& list) {
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
    
    friend std::istream &operator >>(std::istream &is, LinkedList<Traits> &list) {
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

};


template <typename Traits>
LinkedList<Traits>& LinkedList<Traits>::operator=(const LinkedList<Traits>& other){ 
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

template <typename Traits>
void LinkedList<Traits>::clear(){
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

template<typename Traits>
void LinkedList<Traits>::push_back(const value_type& value, Ref ref){
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
template <typename Traits>
void LinkedList<Traits>::internalInsert(const value_type& value, Ref ref, NodePtr& rParent) {
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
#endif // __LINKEDLIST_H__
