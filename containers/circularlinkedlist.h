#ifndef __CIRCULAR_LINKED_LIST_H__
#define __CIRCULAR_LINKED_LIST_H__

#include <concepts>
#include "linkedlist.h"

template <typename Traits = LinkedListAscTraits<TX>>
class CircularLinkedList : public LinkedList<Traits> {
public:
    using Base       = LinkedList<Traits>;
    using value_type = typename Traits::value_type;
    using Node       = typename Traits::Node;
    using NodePtr    = Node*;
    using Compare    = typename Traits::Compare;
    using Delim      = typename Node::Delim;

    CircularLinkedList() : Base() {}

    CircularLinkedList(std::initializer_list<std::pair<value_type, Ref>> values) : Base() {
        for (const auto& v : values) {
            push_back(v.first, v.second);
        }
    }

    CircularLinkedList(const CircularLinkedList& other) : Base() {
        *this = other;
    }

    CircularLinkedList& operator=(const CircularLinkedList& other) {
        if (this != &other) {
            clear();
            std::lock_guard<std::mutex> lockOther(other.m_mutex);
            if (!other.m_pRoot) return *this;
            NodePtr curr = other.m_pRoot;
            do {
                push_back(curr->getValue(), curr->getRef());
                curr = curr->m_pNext;
            } while (curr && curr != other.m_pRoot);
        }
        return *this;
    }

    virtual ~CircularLinkedList() override {
        clear();
    }

    void clear() {
        std::scoped_lock lock(this->m_mutex);
        if (this->m_pTail) {
            this->m_pTail->m_pNext = nullptr; // Rompe el anillo para evitar bucle infinito
        }
        NodePtr curr = this->m_pRoot;
        while (curr) {
            NodePtr next = curr->m_pNext;
            delete curr;
            curr = next;
        }
        this->m_pRoot = nullptr;
        this->m_pTail = nullptr;
    }

    void push_back(const value_type& value, Ref ref) {
        std::scoped_lock lock(this->m_mutex);
        NodePtr new_node = new Node(value, ref, nullptr);
        if (!this->m_pRoot) {
            this->m_pRoot = new_node;
            this->m_pTail = new_node;
            this->m_pTail->m_pNext = this->m_pRoot; // Enlace circular inicial
        } else {
            this->m_pTail->m_pNext = new_node;
            this->m_pTail = new_node;
            this->m_pTail->m_pNext = this->m_pRoot; // Cierra el anillo
        }
    }

    // Reutiliza la inserción ordenada base de LL y reconecta el enlace circular
    void insert(const value_type& value, Ref ref) {
        std::scoped_lock lock(this->m_mutex);
        if (this->m_pTail) {
            this->m_pTail->m_pNext = nullptr; // Temporalmente lineal para inserción ordenada
        }
        this->internalInsert(value, ref, this->m_pRoot);
        
        // Reubica m_pTail al final
        NodePtr curr = this->m_pRoot;
        while (curr && curr->m_pNext) {
            curr = curr->m_pNext;
        }
        this->m_pTail = curr;

        // Cierra el anillo circular en O(1)
        if (this->m_pTail) {
            this->m_pTail->m_pNext = this->m_pRoot;
        }
    }

    // Recorrido de n vueltas en lista circular
    template <typename Func, typename... Args>
    void call_loops(size_t n_vueltas, Func func, Args&&... args) {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        if (!this->m_pRoot || n_vueltas == 0) return;
        NodePtr curr = this->m_pRoot;
        size_t vueltas = 0;
        while (vueltas < n_vueltas) {
            func(*curr, std::forward<Args>(args)...);
            curr = curr->m_pNext;
            if (curr == this->m_pRoot) {
                vueltas++;
            }
        }
    }

    // Sobrecarga de call con parámetro de vueltas
    template <typename Func, std::integral IntType>
    void call(Func func, IntType n_vueltas) {
        call_loops(static_cast<size_t>(n_vueltas), func);
    }

    // Sobrecarga de call() estándar (1 vuelta)
    template <typename Func, typename... Args>
    void call(Func func, Args&&... args) {
        call_loops(1, func, std::forward<Args>(args)...);
    }

    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args&&... args) {
        call(func, std::forward<Args>(args)...);
    }

    template <typename Func, typename... Args>
    Node& FirstThat(Func func, Args&&... args) {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        if (!this->m_pRoot) throw std::runtime_error("Empty list");
        NodePtr curr = this->m_pRoot;
        do {
            if (func(*curr, std::forward<Args>(args)...)) return *curr;
            curr = curr->m_pNext;
        } while (curr != this->m_pRoot);
        throw std::runtime_error("Element not found");
    }

    // Impresión de n vueltas en flujo
    template <typename CharT = char, typename StreamTraits = std::char_traits<CharT>>
    std::basic_ostream<CharT, StreamTraits>& print_loops(
        std::basic_ostream<CharT, StreamTraits>& os, size_t n_vueltas = 1) {
        std::lock_guard<std::mutex> lock(this->m_mutex);
        os << static_cast<CharT>('[');
        bool first = true;
        if (this->m_pRoot && n_vueltas > 0) {
            NodePtr curr = this->m_pRoot;
            size_t vueltas = 0;
            while (vueltas < n_vueltas) {
                if (!first) os << static_cast<CharT>(',');
                os << *curr;
                first = false;
                curr = curr->m_pNext;
                if (curr == this->m_pRoot) {
                    vueltas++;
                }
            }
        }
        return os << static_cast<CharT>(']');
    }

    template <typename CharT, typename StreamTraits = std::char_traits<CharT>>
    friend std::basic_ostream<CharT, StreamTraits>& operator<<(
        std::basic_ostream<CharT, StreamTraits>& os, CircularLinkedList<Traits>& list) {
        return list.print_loops(os, 1);
    }

    template <typename CharT, typename StreamTraits = std::char_traits<CharT>>
    friend std::basic_istream<CharT, StreamTraits>& operator>>(
        std::basic_istream<CharT, StreamTraits>& is, CircularLinkedList<Traits>& list) {
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

#endif // __CIRCULAR_LINKED_LIST_H__