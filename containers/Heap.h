#ifndef __HEAP_H__
#define __HEAP_H__
#include "GeneralTraits.h"
#include "vector.h"
#include "../persistence.h"
#include "containers.h"
#include <cstddef>

template <typename T>
struct HeapNodeTraits {
    using Node = GeneralNode<T>;
    using ForwardIterator   = VectorForwardIterator<Node>;
    using BackwardIterator  = VectorBackwardIterator<Node>;
};

template <typename T, typename _Compare = std::less<T>>
struct HeapAscTraits : public AscendingTraits<T, _Compare>, HeapNodeTraits<T> {};

template <typename T, typename _Compare = std::greater<T>>
struct HeapDescTraits : public DescendingTraits<T, _Compare>, HeapNodeTraits<T> {};

template <typename Traits>
class Heap: public Applicable<Heap<Traits>,Traits> {

public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node *;
    using Compare           = typename Traits::Compare;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using BackwardIterator  = typename Traits::BackwardIterator;
    using Delim             = typename Node::Delim;
private:
    Vector<Traits> m_data;

    // Reconstruye el heap en O(n) (algoritmo de Floyd).
    // Debe llamarse con m_mutex ya bloqueado.
    void rebuild_unlocked() {
        for (size_t i = m_data.size() / 2; i-- > 0; )
            HeapifyDown(i);
    }

public:
    ForwardIterator begin() override { return m_data.begin(); }
    ForwardIterator end() override { return m_data.end();   }

    Heap () {};
    Heap(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            push(v.first, v.second);
    }

    virtual ~Heap() { clear(); }

    std::ostream& write(std::ostream& os) { return os << *this; }
    std::istream& read(std::istream& is) { return is >> *this; }
    //

    void push(const value_type &value, Ref ref) {
        std::scoped_lock lock(this->m_mutex);
        m_data.push_back(value, ref);
        HeapifyUp(m_data.size() - 1);
    }

    void HeapifyUp(size_t index) {
        while (index > 0) {
            size_t parentIndex = (index - 1) / 2;
            if (Compare()(m_data[index].getValue(), m_data[parentIndex].getValue())) {
                std::swap(m_data[index], m_data[parentIndex]);
                index = parentIndex;
            } else {
                break;
            }
        }
    }
    Node pop() {
        //if (m_data.empty()) return;
        std::scoped_lock lock(this->m_mutex);
        auto node_pop = Node(m_data[0].getValue(), m_data[0].getRef());
        std::swap(m_data[0], m_data[m_data.size() - 1]);
        m_data.pop_back();
        HeapifyDown(0);
        return node_pop;
    }

    void HeapifyDown(size_t index) {
        size_t childIndex[2] {(index + 1) * 2 - 1, (index + 1) * 2};
        while (childIndex[0] < m_data.size()) {
            auto branch = 0;
            if (childIndex[1] < m_data.size()) {
                branch = Compare()(m_data[childIndex[1]].getValue(), m_data[childIndex[0]].getValue());
            }
            if (Compare()(m_data[childIndex[branch]].getValue(), m_data[index].getValue())) {
                std::swap(m_data[childIndex[branch]], m_data[index]);
                index = childIndex[branch];
                childIndex[0] = (index + 1) * 2 - 1;
                childIndex[1] = (index + 1) * 2;
            } else {
                break;
            }
        }
    }

    void clear() {
        std::scoped_lock lock(this->m_mutex);
        m_data.clear();
    }

    // Reconstruye el heap por si los valores se modificaron externamente.
    void rebuild() {
        std::scoped_lock lock(this->m_mutex);
        rebuild_unlocked();
    }

    // Aplica una funcion a todos los nodos. Como el callback puede modificar
    // los valores y romper la propiedad del heap, se reconstruye al final.
    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args&&... args) {
        std::scoped_lock lock(this->m_mutex);
        try {
            ::call(begin(), end(), func, std::forward<Args>(args)...);
        } catch (...) {
            rebuild_unlocked();
            throw;
        }
        rebuild_unlocked();
    }

};

template <typename T, typename Traits=HeapAscTraits<T>>
class MinHeap : public Heap<Traits> {

};

template <typename T, typename Traits=HeapDescTraits<T>>
class MaxHeap : public Heap<Traits> {

};



#endif // __HEAP_H__
