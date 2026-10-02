#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include <istream>
#include <mutex>
#include <sstream>
#include <string>
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "../foreach.h"

template <typename T>
class LinkedListNode : public GeneralNode<T> {
    using Node    = LinkedListNode<T>;
    using NodePtr = Node *;
public:
    Node *m_pNext = nullptr; // puntero al siguiente nodo
public:
    LinkedListNode(const T& value, Ref ref, Node *pNext) : GeneralNode<T>(value, ref), m_pNext(pNext){}
    // TODO: El operator<< deberia ir en GeneralNode, no en LinkedListNode, para que sea generico y reusable.
    /*friend std::ostream &operator <<(std::ostream &os, const LinkedListNode<T> &node) {
        os << "(" << node.getValue() << "," << node.getRef() << ")";
        return os;
    }*/
    Node* getM_pNext() { return m_pNext; }
};

template <typename T>
class LinkedListForwardIterator : public GeneralIterator<LinkedListForwardIterator<T>, LinkedListNode<T>> {
public:
    using value_type        = LinkedListNode<T>;
    using MySelf            = LinkedListForwardIterator<T>;
    using Parent            = GeneralIterator<MySelf, value_type>;
    using Parent::Parent; // Inherit constructor
    LinkedListForwardIterator operator++() { Parent::m_ptr = Parent::m_ptr->m_pNext; return *this; }
};

template <typename T>
struct LinkedListAscTraits {
    using value_type        = T;
    using Node              = LinkedListNode<T>;
    using ForwardIterator   = LinkedListForwardIterator<T>;  // itera sobre Node, no sobre T
};

template <typename Traits>
class LinkedList {
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node *;
    using ForwardIterator   = typename Traits::ForwardIterator;
private:
    NodePtr m_pRoot = nullptr; // puntero al primer nodo de la lista enlazada
    NodePtr m_pTail = nullptr; // puntero al último nodo de la lista enlazada
    // TODO: agregar mutex para sincronización de acceso concurrente
    std::mutex m_mutex;             // mutex para sincronización

    NodePtr NodePtrGetRoot() const { return m_pRoot; }
    NodePtr NodePtrGetTail() const { return m_pTail; }
public:
    LinkedList() {}
    // TODO: implementar LinkedList con nodos enlazados y métodos push_back.
    LinkedList(const LinkedList&)            ; // no se permite copia
    // TODO: implementar LinkedList con nodos enlazados y métodos push_back.
    LinkedList& operator==(const LinkedList&); // no se permite asignacion

    // TODO: implementar la destruccion en un metodo clear()
    void clear();
    // TODO: implementar destructor para liberar memoria de forma segura
    virtual ~LinkedList();

    // TODO: implementar métodos de iteración, push_back, etc.
    void push_back(const value_type& value, Ref ref);

private:
    void internalInsert(const value_type& value, Ref ref, NodePtr&rParent);
public:
    // TODO: implementar insert() para LinkedList
    void insert(const value_type& value, Ref ref){ internalInsert(value, ref, m_pRoot); }

    // TODO: persistencia: write() y read() para LinkedList
    std::ostream &write(std::ostream &os) { return os << *this; }
    std::istream &read(std::istream &is)  { return is >> *this; }
    friend std::ostream &operator <<(std::ostream &os, const LinkedList<Traits> &list) {
        NodePtr current = list.m_pRoot;
        os << "[";
        while (current != nullptr) {
            os << *current;
            current = current->m_pNext;
            if (current != nullptr) os << ",";
        }
        return os << "]";
    }
    // TODO: implementar
    friend std::istream &operator >>(std::istream &is, LinkedList<Traits> &list) {
        list.clear();
        value_type m_value;
        string str;
        Ref m_ref;
        getline(is, str);
        for (size_t i = 0; i < str.size(); i++) {
            if (str[i] == '[' || str[i] == ']' || str[i] == '(' || str[i] == ')' || str[i] == ',') {
                str[i] = ' ';
            }
        }
        stringstream ss(str);
        while (ss >> m_value >> m_ref) {
            list.push_back(m_value, m_ref);
        }
        return is;
    }
    // Iterators
    ForwardIterator begin() { return ForwardIterator(m_pRoot); }
    ForwardIterator end()   { return ForwardIterator(nullptr); }

    // TODO: implementar ApplyFunction(), FirstThat(), call, rcall para LinkedList
    // Chequear que hago para evitar codigo repetido
};

template <typename Traits>
LinkedList<Traits>::~LinkedList(){
    clear();
}

template <typename Traits>
LinkedList<Traits>::LinkedList(const LinkedList& other) {
    *this == other;
}

template <typename Traits>
LinkedList<Traits>& LinkedList<Traits>::operator==(const LinkedList<Traits>& other){ // no se permite asignacion
    clear();
    auto current = other.m_pRoot;
    while(current != nullptr) {
        this->push_back(current->getValue(), current->getRef());
        current = current->m_pNext;
    }
    return *this;
}

// TODO: explicar recursividad de cola de llamadas en insert() y internalInsert()
template <typename Traits>
void LinkedList<Traits>::internalInsert(const value_type& value, Ref ref, NodePtr &rParent){
    if( rParent == nullptr || value < rParent->getValue() ) {
        rParent = new Node(value, ref, rParent);
        if (rParent->m_pNext == nullptr) m_pTail = rParent;
        return;
    }
    internalInsert(value, ref, rParent->m_pNext);
}

template <typename Traits>
void LinkedList<Traits>::clear() {
    auto current = m_pRoot;
    while(current != nullptr) {
        current = current->m_pNext;
        delete m_pRoot;
        m_pRoot = current;
    }
    m_pTail = nullptr;
}

template <typename Traits>
void LinkedList<Traits>::push_back(const value_type& value, Ref ref) {
    if (m_pRoot == nullptr) {
        m_pRoot = new Node(value, ref, nullptr);
        m_pTail = m_pRoot;
        return;
    }
    m_pTail->m_pNext = new Node(value, ref, nullptr);
    m_pTail = m_pTail->m_pNext;
}
#endif // __LINKEDLIST_H__
