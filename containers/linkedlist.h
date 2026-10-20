#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include <mutex>
#include <initializer_list>
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralContainer.h"
#include "stream.h"
#include "traits.h"

template <typename T, typename Derived = void>
class LinkedListNode : public GeneralNode<T> {
    using Self = LinkedListNode<T, Derived>;
public:
    using Node = conditional_t<is_void_v<Derived>, Self, Derived>;
    Node* m_pNext = nullptr; // puntero al siguiente nodo, npara acceso directo necesita ser publico
    LinkedListNode() : GeneralNode<T>(T{}, Ref{}), m_pNext(nullptr) {}
    // añadir friend class añade otro typename al template
    LinkedListNode(const T& value, Ref ref, Node* pNext) : GeneralNode<T>(value, ref), m_pNext(pNext) {}
};

template <typename Node, bool Circular = false, auto Link = &Node::m_pNext>
class LinkedListIterator : public GeneralIterator<LinkedListIterator<Node, Circular, Link>, Node> {
public:
    using value_type = Node;
    using MySelf = LinkedListIterator<Node, Circular, Link>;
    using Parent = GeneralIterator<MySelf, value_type>;
private:
    Node* m_pStart;
public:
    LinkedListIterator(Node* ptr) : Parent(ptr), m_pStart(ptr) {}
    LinkedListIterator& operator++() {
        Parent::m_ptr = Parent::m_ptr->*Link;
        if (Circular && Parent::m_ptr == m_pStart)
            Parent::m_ptr = nullptr;
        return *this;
    }
};

template <typename T>
struct LinkedListStructure {
    using Node              = LinkedListNode<T>;
    using ForwardIterator   = LinkedListIterator<Node>;  // itera sobre Node, no sobre T
    static constexpr bool circular = false;
    static constexpr bool doubly   = false;
};

template <typename T>
using LinkedListAscTraits = ListTraits<AscendingTraits<T>, LinkedListStructure<T>>;

template <typename T>
using LinkedListDescTraits = ListTraits<DescendingTraits<T>, LinkedListStructure<T>>;

template <typename Traits, typename Derived = void>
class LinkedList : public GeneralContainer<conditional_t<is_void_v<Derived>, LinkedList<Traits>, Derived>> {
    using Base = GeneralContainer<conditional_t<is_void_v<Derived>, LinkedList<Traits>, Derived>>;
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node *;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using Compare           = typename Traits::Compare;
private:
    NodePtr m_pRoot = nullptr; // puntero al primer nodo de la lista enlazada
    NodePtr m_pTail = nullptr; // puntero al último nodo de la lista enlazada
    
    Compare m_comp; // comparador para ordenar los nodos de la lista
    using Base::m_mutex;

    void internalInsert(const value_type& value, Ref ref);
    void internalPushBack(const value_type& value, Ref ref);
    void internalLink(NodePtr pred, NodePtr node, NodePtr succ);
    void internalClear();
    void append(initializer_list<pair<value_type, Ref>> values);
    void copyFrom(const LinkedList& another);

protected:
    NodePtr GetTail() const { return m_pTail; }

public:
    LinkedList() {}
    LinkedList(const LinkedList& another) : LinkedList() { copyFrom(another); } // copia profunda de la lista enlazada
    LinkedList& operator=(const LinkedList& another){ copyFrom(another); return *this; }
    LinkedList(initializer_list<pair<value_type, Ref>> values) : LinkedList() { append(values); }

    void clear();
    virtual ~LinkedList(){ clear(); };

    void push_back(const value_type& value, Ref ref);

    bool empty() const { lock_guard<mutex> lock(m_mutex); return m_pRoot == nullptr; }

    void insert(const value_type& value, Ref ref) {
        scoped_lock lock(m_mutex);
        internalInsert(value, ref);
    }

    std::ostream& write(std::ostream& os) const {
        lock_guard lock(m_mutex);
        auto first = true;
        os << "[";
        for (auto it = begin(); it != end(); ++it){
            if (!first)
                os << ",";
            os << *it;
            first = false;
        }
        return os << "]";
    }
    std::istream& read(std::istream& is) { return ReadContainer(is, *this); }

    // Iterators
    // ForwardIterator begin() { return ForwardIterator(m_pRoot); }
    // ForwardIterator end() { return ForwardIterator(nullptr); }
    ForwardIterator begin() const { return ForwardIterator(m_pRoot); }
    ForwardIterator end() const { return ForwardIterator(nullptr); }
};

template <typename Traits, typename Derived>
void LinkedList<Traits, Derived>::copyFrom(const LinkedList& another){
    if (this == &another)
        return;
    scoped_lock lock(m_mutex, another.m_mutex);
    internalClear();
    m_comp = another.m_comp;
    for (auto it = another.begin(); it != another.end(); ++it)
        internalPushBack((*it).getValue(), (*it).getRef());
}

template <typename Traits, typename Derived>
void LinkedList<Traits, Derived>::append(initializer_list<pair<value_type, Ref>> values){
    for (const auto &v : values)
        push_back(v.first, v.second);
}

template <typename Traits, typename Derived>
void LinkedList<Traits, Derived>::clear(){
    scoped_lock lock(m_mutex);
    internalClear();
}

template <typename Traits, typename Derived>
void LinkedList<Traits, Derived>::internalClear(){
    NodePtr last = m_pTail;
    NodePtr curr = m_pRoot;
    m_pRoot = nullptr;
    m_pTail = nullptr;
    while (curr){
        NodePtr next = (curr == last) ? nullptr : curr->m_pNext;
        delete curr;
        curr = next;
    }
}

template <typename Traits, typename Derived>
void LinkedList<Traits, Derived>::push_back(const value_type& value, Ref ref){
    scoped_lock lock(m_mutex);
    internalPushBack(value, ref);
}

template <typename Traits, typename Derived>
void LinkedList<Traits, Derived>::internalPushBack(const value_type& value, Ref ref){
    internalLink(m_pTail, new Node(value, ref, nullptr), nullptr);
}

template <typename Traits, typename Derived>
void LinkedList<Traits, Derived>::internalLink(NodePtr pred, NodePtr node, NodePtr succ){
    node->m_pNext = succ;
    if (pred)
        pred->m_pNext = node;
    else
        m_pRoot = node;
    if (!succ)
        m_pTail = node;
    if constexpr (Traits::doubly) {
        node->m_pPrev = pred;
        if (succ)
            succ->m_pPrev = node;
    }
    if constexpr (Traits::circular) {
        m_pTail->m_pNext = m_pRoot;
        if constexpr (Traits::doubly)
            m_pRoot->m_pPrev = m_pTail;
    }
}

template <typename Traits, typename Derived>
void LinkedList<Traits, Derived>::internalInsert(const value_type& value, Ref ref) {
    NodePtr pred = nullptr;
    NodePtr succ = nullptr;
    for (auto it = begin(); it != end(); ++it) {
        if (m_comp(value, (*it).getValue())) {
            succ = &*it;
            break;
        }
        pred = &*it;
    }
    internalLink(pred, new Node(value, ref, nullptr), succ);
}
#endif // __LINKEDLIST_H__
