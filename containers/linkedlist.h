#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include <mutex>
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "../foreach.h"

template <typename Traits> struct LinkedListAscTraits;
template <typename Traits> struct LinkedListDescTraits;

template <typename Traits>
class LinkedListNode : public GeneralNode<typename Traits::value_type> {
public:
    using value_type = typename Traits::value_type;
    using Node = typename Traits::Node;
    using NodePtr = Node*;
    Node* m_pNext = nullptr; // puntero al siguiente nodo, npara acceso directo necesita ser publico
    LinkedListNode() : GeneralNode<value_type>(value_type{}, Ref{}), m_pNext(nullptr) {}
    // añadir friend class añade otro typename al template
    LinkedListNode(const value_type& value, Ref ref, Node* pNext = nullptr) : GeneralNode<value_type>(value, ref), m_pNext(pNext) {}
};

template <typename Traits>
class LinkedListForwardIterator : public GeneralIterator<LinkedListForwardIterator<Traits>, typename Traits::Node> {
public:
    using value_type = typename Traits::Node;
    using MySelf = LinkedListForwardIterator<Traits>;
    using Parent = GeneralIterator<MySelf, value_type>;
    using Parent::Parent; // Inherit constructor
    LinkedListForwardIterator& operator++() { Parent::m_ptr = Parent::m_ptr->m_pNext; return *this; }
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
    using Node              = LinkedListNode<LinkedListAscTraits<T>>;
    using ForwardIterator   = LinkedListForwardIterator<LinkedListAscTraits<T>>;  // itera sobre Node, no sobre T
};

template <typename T>
struct LinkedListDescTraits : public DescendingTraits<T> {
    using Node              = LinkedListNode<LinkedListDescTraits<T>>;
    using ForwardIterator   = LinkedListForwardIterator<LinkedListDescTraits<T>>;  // itera sobre Node, no sobre T
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

    void clear();
    virtual ~LinkedList(){ clear(); };

    void push_back(const value_type& value, Ref ref);

    bool empty() const { return m_pRoot == nullptr; }

    void insert(const value_type& value, Ref ref) {
        scoped_lock lock(m_mutex);
        internalInsert(value, ref, m_pRoot);
    }

    template <typename CharT, typename StreamTraits = std::char_traits<CharT>>
    std::basic_ostream<CharT, StreamTraits>& write(std::basic_ostream<CharT, StreamTraits>& os) { return os << *this; }
    template <typename CharT, typename StreamTraits = std::char_traits<CharT>>
    std::basic_istream<CharT, StreamTraits>& read(std::basic_istream<CharT, StreamTraits>& is) { return is >> *this; }

    template <typename CharT, typename StreamTraits = std::char_traits<CharT>>
    friend std::basic_ostream<CharT, StreamTraits>& operator <<(
        std::basic_ostream<CharT, StreamTraits>& os, const LinkedList<Traits>& list) {
        std::lock_guard lock(list.m_mutex);
        auto first = true;
        os << static_cast<CharT>('[');
        for (auto it = list.begin(); it != list.end(); ++it){
            if (!first)
                os << static_cast<CharT>(',');
            os << *it;
            first = false;
        }
        return os << static_cast<CharT>(']');
    }
        
    template <typename CharT, typename StreamTraits = std::char_traits<CharT>>
    friend std::basic_istream<CharT, StreamTraits> &operator >>(
        std::basic_istream<CharT, StreamTraits> &is, LinkedList<Traits> &list) {
        CharT d;
        Node node;
        list.clear();

        is >> d;
        if (is >> d && d != static_cast<CharT>(']')){
            is.unget();
            while(is >> node >> d){
                list.push_back(node.getValue(), node.getRef());
                if (d == static_cast<CharT>(']')){
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

    NodePtr next = other.m_pRoot->m_pNext;
    NodePtr curr = m_pRoot;

    while(next){
        curr->m_pNext = new Node(next->getValue(), next->getRef(), nullptr);
        curr = curr->m_pNext;
        next = next->m_pNext;
    }
    m_pTail = curr;
    return *this;
}

template <typename Traits>
void LinkedList<Traits>::clear(){
    scoped_lock lock(m_mutex);
    Node* curr = m_pRoot;
    while (curr) {
        Node* next = curr->m_pNext;
        delete curr;
        curr = next;
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
    if (rParent == nullptr || value < rParent->getValue()) {
        rParent = new Node(value, ref, rParent);
        m_pTail = rParent;
        return;
    }
    internalInsert(value, ref, rParent->m_pNext);
}
#endif // __LINKEDLIST_H__
