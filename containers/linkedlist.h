#ifndef __LINKEDLIST_H__
#define __LINKEDLIST_H__
#include <mutex>
#include <vector>
#include <ranges>
#include <initializer_list>
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "traits.h"
#include "../foreach.h"

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

template <typename Node>
class LinkedListForwardIterator : public GeneralIterator<LinkedListForwardIterator<Node>, Node> {
public:
    using value_type = Node;
    using MySelf = LinkedListForwardIterator<Node>;
    using Parent = GeneralIterator<MySelf, value_type>;
    using Parent::Parent; // Inherit constructor
    LinkedListForwardIterator& operator++() { Parent::m_ptr = Parent::m_ptr->m_pNext; return *this; }
};

template <typename T>
struct LinkedListStructure {
    using Node              = LinkedListNode<T>;
    using ForwardIterator   = LinkedListForwardIterator<Node>;  // itera sobre Node, no sobre T
    static constexpr bool circular = false;
    static constexpr bool doubly   = false;
};

template <typename T>
using LinkedListAscTraits = ListTraits<AscendingTraits<T>, LinkedListStructure<T>>;

template <typename T>
using LinkedListDescTraits = ListTraits<DescendingTraits<T>, LinkedListStructure<T>>;

template <typename Traits>
class LinkedList {
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node *;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using Compare           = typename Traits::Compare;
    using Delim             = typename Node::Delim;
private:
    NodePtr m_pRoot = nullptr; // puntero al primer nodo de la lista enlazada
    NodePtr m_pTail = nullptr; // puntero al último nodo de la lista enlazada
    
    Compare m_comp; // comparador para ordenar los nodos de la lista
    mutable std::mutex m_mutex; // mutex para sincronización

    void internalInsert(const value_type& value, Ref ref);
    void internalPushBack(const value_type& value, Ref ref);
    void internalClear();

protected:
    NodePtr GetRoot() const { return m_pRoot; }
    NodePtr GetTail() const { return m_pTail; }
    unique_lock<mutex> Lock() const { return unique_lock<mutex>(m_mutex); }
    void append(initializer_list<pair<value_type, Ref>> values);
    void copyFrom(const LinkedList& another);
    virtual void internalLink(NodePtr pred, NodePtr node, NodePtr succ);

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

    std::ostream& write(std::ostream& os) const { return os << *this; }
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

    template<typename Func, typename... Args>
    decltype(auto) rcall(Func func, Args&&... args)
    {    lock_guard<mutex> lock(m_mutex);
        vector<NodePtr> nodes;
        for (auto it = begin(); it != end(); ++it)
            nodes.push_back(&*it);
        auto reversed = nodes | views::reverse | views::transform([](NodePtr p) -> Node& { return *p; });
        return ::call(reversed.begin(), reversed.end(), std::move(func), std::forward<Args>(args)...);
    }
};

template <typename Traits>
void LinkedList<Traits>::copyFrom(const LinkedList<Traits>& another){
    if (this == &another)
        return;
    scoped_lock lock(m_mutex, another.m_mutex);
    internalClear();
    m_comp = another.m_comp;
    for (auto it = another.begin(); it != another.end(); ++it)
        internalPushBack((*it).getValue(), (*it).getRef());
}

template <typename Traits>
void LinkedList<Traits>::append(initializer_list<pair<value_type, Ref>> values){
    for (const auto &v : values)
        push_back(v.first, v.second);
}

template <typename Traits>
void LinkedList<Traits>::clear(){
    scoped_lock lock(m_mutex);
    internalClear();
}

template <typename Traits>
void LinkedList<Traits>::internalClear(){
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

template<typename Traits>
void LinkedList<Traits>::push_back(const value_type& value, Ref ref){
    scoped_lock lock(m_mutex);
    internalPushBack(value, ref);
}

template <typename Traits>
void LinkedList<Traits>::internalPushBack(const value_type& value, Ref ref){
    internalLink(m_pTail, new Node(value, ref, nullptr), nullptr);
}

template <typename Traits>
void LinkedList<Traits>::internalLink(NodePtr pred, NodePtr node, NodePtr succ){
    node->m_pNext = succ;
    if (pred)
        pred->m_pNext = node;
    else
        m_pRoot = node;
    if (!succ)
        m_pTail = node;
}

template <typename Traits>
void LinkedList<Traits>::internalInsert(const value_type& value, Ref ref) {
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
