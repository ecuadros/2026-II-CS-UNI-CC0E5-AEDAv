#ifndef __DOUBLY_LINKED_LIST_H__
#define __DOUBLY_LINKED_LIST_H__

#include "DoublyLinkedListNode.h"
#include "GeneralIterator.h"
#include "linkedlist.h"

// BackwardIterator 
template <typename Node>
class DoublyLinkedListBackwardIterator 
    : public GeneralIterator<DoublyLinkedListBackwardIterator<Node>, Node> {
public:
    using value_type = Node;
    using MySelf     = DoublyLinkedListBackwardIterator<Node>;
    using Parent     = GeneralIterator<MySelf, value_type>;
    using Parent::Parent;

    DoublyLinkedListBackwardIterator& operator++() {
        Parent::m_ptr = Parent::m_ptr->m_pPrev; 
        return *this;
    }
};

// Traits de Lista Doble
template <typename T>
struct DoublyLinkedListAscTraits : public AscendingTraits<T> {
    using Node             = DoublyLinkedListNode<DoublyLinkedListAscTraits<T>>;
    using ForwardIterator  = LinkedListForwardIterator<DoublyLinkedListAscTraits<T>>;        // <-- Reutilizado de LL
    using BackwardIterator = DoublyLinkedListBackwardIterator<Node>; // <-- Especializado LDE
};

template <typename T>
struct DoublyLinkedListDescTraits : public DescendingTraits<T> {
    using Node             = DoublyLinkedListNode<DoublyLinkedListDescTraits<T>>;
    using ForwardIterator  = LinkedListForwardIterator<DoublyLinkedListDescTraits<T>>;        // <-- Reutilizado de LL
    using BackwardIterator = DoublyLinkedListBackwardIterator<Node>; // <-- Especializado LDE
};

// Contenedor DoublyLinkedList (LDE)
template <typename Traits = DoublyLinkedListAscTraits<TX>>
class DoublyLinkedList : public LinkedList<Traits> {
public:
    using Base             = LinkedList<Traits>;
    using value_type       = typename Traits::value_type;
    using Node             = typename Traits::Node;
    using NodePtr          = Node*;
    using ForwardIterator  = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
    using Compare          = typename Traits::Compare;
    using Delim            = typename Node::Delim;

    DoublyLinkedList() : Base() {}

    DoublyLinkedList(std::initializer_list<std::pair<value_type, Ref>> values) : Base() {
        for (const auto& v : values) {
            push_back(v.first, v.second);
        }
    }

    DoublyLinkedList(const DoublyLinkedList& other) : Base() {
        *this = other;
    }

    DoublyLinkedList& operator=(const DoublyLinkedList& other) {
        if (this != &other) {
            this->clear();
            std::lock_guard<std::mutex> lockOther(other.m_mutex);
            for (NodePtr curr = other.m_pRoot; curr != nullptr; curr = curr->m_pNext) {
                push_back(curr->getValue(), curr->getRef());
            }
        }
        return *this;
    }

    virtual ~DoublyLinkedList() override = default;

    void push_back(const value_type& value, Ref ref) {
        std::scoped_lock lock(this->m_mutex);
        NodePtr new_node = new Node(value, ref, nullptr, this->m_pTail);
        if (!this->m_pRoot) {
            this->m_pRoot = new_node;
            this->m_pTail = new_node;
        } else {
            this->m_pTail->m_pNext = new_node;
            new_node->m_pPrev = this->m_pTail;
            this->m_pTail = new_node;
        }
    }

    // Reutiliza la inserción ordenada base de LL y sincroniza los enlaces m_pPrev
    void insert(const value_type& value, Ref ref) {
        std::scoped_lock lock(this->m_mutex);
        this->internalInsert(value, ref, this->m_pRoot);

        // Sincronización lineal de m_pPrev y m_pTail tras la inserción
        NodePtr prev = nullptr;
        NodePtr curr = this->m_pRoot;
        while (curr) {
            curr->m_pPrev = prev;
            prev = curr;
            curr = curr->m_pNext;
        }
        this->m_pTail = prev;
    }

    // Iteradores inversos (Rúbrica oficial image.png)
    BackwardIterator rbegin() const { return BackwardIterator(this->m_pTail); }
    BackwardIterator rend()   const { return BackwardIterator(nullptr); }

    // Sobrecarga de rcall() para recorrido inverso en O(1) por paso
    template <typename Func, typename... Args>
    decltype(auto) rcall(Func func, Args&&... args) {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        if constexpr(std::is_void_v<std::invoke_result_t<Func, Node&, Args...>>) {
            ::call(rbegin(), rend(), std::forward<Func>(func), std::forward<Args>(args)...);
        } else {
            return ::call(rbegin(), rend(), std::forward<Func>(func), std::forward<Args>(args)...);
        }
    }

    // Diagnóstico visual bidireccional en flujos de salida
    template <typename CharT, typename StreamTraits = std::char_traits<CharT>>
    friend std::basic_ostream<CharT, StreamTraits>& operator<<(
        std::basic_ostream<CharT, StreamTraits>& os, DoublyLinkedList<Traits>& list) {
        std::lock_guard<std::mutex> lock(list.m_mutex);
        os << static_cast<CharT>('[');
        bool first = true;
        for (NodePtr curr = list.m_pRoot; curr != nullptr; curr = curr->m_pNext) {
            if (!first) os << static_cast<CharT>(',');
            os << *curr;
            first = false;
        }
        os << static_cast<CharT>(']') << " |bwd:[" ;
        first = true;
        for (NodePtr curr = list.m_pTail; curr != nullptr; curr = curr->m_pPrev) {
            if (!first) os << static_cast<CharT>(',');
            os << *curr;
            first = false;
        }
        return os << static_cast<CharT>(']');
    }

    template <typename CharT, typename StreamTraits = std::char_traits<CharT>>
    friend std::basic_istream<CharT, StreamTraits>& operator>>(
        std::basic_istream<CharT, StreamTraits>& is, DoublyLinkedList<Traits>& list) {
        CharT d;
        Node node;
        list.clear();

        is >> d;
        if (is >> d && d != static_cast<CharT>(']')) {
            is.unget();
            while (is >> node >> d) {
                list.push_back(node.getValue(), node.getRef());
                if (d == static_cast<CharT>(']')) break;
            }
        }
        return is;
    }
};

#endif // __DOUBLY_LINKED_LIST_H__