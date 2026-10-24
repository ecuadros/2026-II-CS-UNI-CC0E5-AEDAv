#ifndef __HEAP_H__
#define __HEAP_H__
#include <mutex>
#include <stdexcept>
#include "vector.h"
#include "GeneralContainer.h"
#include "stream.h"
#include "traits.h"

template <typename T>
struct HeapStructure {
    using Node              = GeneralNode<T>;
    using Container         = Vector<VectorTraits<T>>;
};

template <typename T>
using MinHeapTraits = ListTraits<AscendingTraits<T>, HeapStructure<T>>;

template <typename T>
using MaxHeapTraits = ListTraits<DescendingTraits<T>, HeapStructure<T>>;

template <typename Traits>
class Heap : public GeneralContainer<Heap<Traits>> {
    using Base = GeneralContainer<Heap<Traits>>;
    friend Base;
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using Container         = typename Traits::Container;
    using Compare           = typename Traits::Compare;
private:
    Container     m_data;
    Compare       m_comp;
    using Base::m_mutex;

    void HeapifyUp(size_t index) {
        while (index > 0) {
            size_t parentIndex = (index - 1) / 2;
            if (m_comp(m_data[index].getValue(), m_data[parentIndex].getValue())) {
                std::swap(m_data[index], m_data[parentIndex]);
                index = parentIndex;
            } else {
                break;
            }
        }
    }

    void HeapifyDown(size_t index) {
        size_t n = m_data.size();
        while (true) {
            size_t left  = 2 * index + 1;
            size_t right = 2 * index + 2;
            size_t best  = index;
            if (left  < n && m_comp(m_data[left].getValue(),  m_data[best].getValue()))
                best = left;
            if (right < n && m_comp(m_data[right].getValue(), m_data[best].getValue()))
                best = right;
            if (best == index)
                break;
            std::swap(m_data[index], m_data[best]);
            index = best;
        }
    }

    void AfterApply() {
        for (size_t i = m_data.size() / 2; i-- > 0; )
            HeapifyDown(i);
    }

public:
    void push_back(const value_type &value, Ref ref) {
        lock_guard<mutex> lock(m_mutex);
        m_data.push_back(value, ref);
        HeapifyUp(m_data.size() - 1);
    }

    Node top() {
        lock_guard<mutex> lock(m_mutex);
        if (m_data.empty()) throw std::out_of_range("top(): el heap esta vacio");
        return m_data[0];
    }

    Node pop() {
        lock_guard<mutex> lock(m_mutex);
        if (m_data.empty()) throw std::out_of_range("pop(): el heap esta vacio");
        Node root = m_data[0];
        std::swap(m_data[0], m_data[m_data.size() - 1]);
        m_data.pop_back();
        HeapifyDown(0);
        return root;
    }

    size_t size()  const { lock_guard<mutex> lock(m_mutex); return m_data.size();  }
    bool   empty() const { lock_guard<mutex> lock(m_mutex); return m_data.empty(); }

    void clear() {
        lock_guard<mutex> lock(m_mutex);
        m_data.clear();
    }

    auto begin() const { return m_data.begin(); }
    auto end()   const { return m_data.end();   }
};

template <typename T>
using MinHeap = Heap<MinHeapTraits<T>>;

template <typename T>
using MaxHeap = Heap<MaxHeapTraits<T>>;

#endif // __HEAP_H__
