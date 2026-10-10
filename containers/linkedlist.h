#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include <mutex>
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "../foreach.h"

template <typename T>
class LinkedListNode : public GeneralNode<T> {
    using Node = LinkedListNode<T>;
    using NodePtr = Node*;
public:
    Node* m_pNext = nullptr; // puntero al siguiente nodo, npara acceso directo necesita ser publico
    LinkedListNode() : GeneralNode<T>(T{}, Ref{}), m_pNext(nullptr) {}
    // añadir friend class añade otro typename al template
    LinkedListNode(const T& value, Ref ref, Node* pNext) : GeneralNode<T>(value, ref), m_pNext(pNext) {}
};

template <typename T, typename NodeType = LinkedListNode<T>>
class LinkedListForwardIterator : public GeneralIterator<LinkedListForwardIterator<T, NodeType>, NodeType> {
public:
    using value_type = NodeType;
    using MySelf = LinkedListForwardIterator<T, NodeType>;
    using Parent = GeneralIterator<MySelf, value_type>;
private:
    value_type* m_start; 
public:
    
    LinkedListForwardIterator(value_type* ptr) : Parent(ptr), m_start(ptr) {}
    LinkedListForwardIterator& operator++() { 
        auto next = Parent::m_ptr->m_pNext;
        Parent::m_ptr = static_cast<value_type*>(next); 
        if(Parent::m_ptr == m_start) 
            Parent::m_ptr = nullptr;
        return *this; 
    }
};

template <typename T, typename _Compare>
struct DefaultTraits {
    using value_type        = T;
    using Compare           = _Compare;
};
template <typename T, typename _Compare = std::less<T>>
struct AscendingTraits : public DefaultTraits<T, _Compare> {};

template <typename T, typename _Compare = std::greater<T>>
struct DescendingTraits : public DefaultTraits<T, _Compare> {};
template <typename T>
struct LinkedListAscTraits : public AscendingTraits<T> {
    using Node              = LinkedListNode<T>;
    using ForwardIterator   = LinkedListForwardIterator<T>;  // itera sobre Node, no sobre T
};

template <typename T>
struct LinkedListDescTraits : public DescendingTraits<T> {
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
    using Compare           = typename Traits::Compare;
    using Delim             = typename Node::Delim;
protected:
    NodePtr m_pRoot = nullptr; // puntero al primer nodo de la lista enlazada
    NodePtr m_pTail = nullptr; // puntero al último nodo de la lista enlazada
    
    Compare m_comp; // comparador para ordenar los nodos de la lista
    mutable std::mutex m_mutex; // mutex para sincronización

    NodePtr GetRoot() const { return m_pRoot; }
    void internalInsert(const value_type& value, Ref ref, NodePtr& rParent);

public:
    LinkedList() {}
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
        // if(list.m_pTail != nullptr && list.m_pRoot != list.m_pTail && list.m_pTail->m_pNext == list.m_pRoot)
        //      os << "," << *list.m_pRoot;
        //
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

    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args... args) {
        call(func, std::forward<Args>(args)...);
    }
    template <typename Func, typename... Args>
    Node& FirstThat(Func func, Args... args) {
        return call(func, std::forward<Args>(args)...);
    }
    template<typename Func, typename... Args>
    decltype(auto) call(Func func, Args&&... args)
    {    lock_guard<mutex> lock(m_mutex);
        if constexpr(is_void_v<invoke_result_t<Func, Node&, Args...>>)
            ::call(begin(), end(), std::forward<Func>(func), std::forward<Args>(args)...);
        else // return type is not void:
            return ::call(begin(), end(), std::forward<Func>(func), std::forward<Args>(args)...);
    }
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

    NodePtr next = static_cast<NodePtr>(other.m_pRoot->m_pNext);
    NodePtr curr = m_pRoot;

    while(next != nullptr && next != other.m_pRoot){
        curr->m_pNext = new Node(next->getValue(), next->getRef(), nullptr);
        curr = static_cast<NodePtr>(curr->m_pNext);
        next = static_cast<NodePtr>(next->m_pNext);
    }
    m_pTail = curr;

    if(other.m_pTail != nullptr && other.m_pTail->m_pNext == other.m_pRoot)
        this->m_pTail->m_pNext = this->m_pRoot; 
    return *this;
}

template <typename Traits>
void LinkedList<Traits>::clear(){
    scoped_lock lock(m_mutex);
    NodePtr current = m_pRoot;
    while (current != nullptr) {
        NodePtr next = static_cast<NodePtr>(current->m_pNext);
        delete current;
        current = next;
    }
    m_pRoot = nullptr;
    m_pTail = nullptr;
}

template<typename Traits>
void LinkedList<Traits>::push_back(const value_type& value, Ref ref){
    scoped_lock lock(m_mutex);
    NodePtr new_node = new Node(value, ref, nullptr);
    if (!this->m_pRoot){
        this->m_pRoot = new_node;
        this->m_pTail = new_node;
    } else {
        this->m_pTail->m_pNext = new_node;
        this->m_pTail = new_node;
    }
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

    if (rParent == m_pTail && !m_comp(value, rParent->getValue())) {
        NodePtr new_node = new Node(value, ref, nullptr);
        rParent->m_pNext = new_node;
        m_pTail = new_node;
        return;
    }

    NodePtr next = static_cast<NodePtr>(rParent->m_pNext);
    internalInsert(value, ref, next);
    rParent->m_pNext = next;
}
#endif // __LINKEDLIST_H__
