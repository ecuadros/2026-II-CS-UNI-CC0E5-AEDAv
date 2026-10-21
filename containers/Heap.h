#ifndef __HEAP_H__
#define __HEAP_H__
#include <mutex>
#include <algorithm>
#include <stdexcept>
#include "Traits.h"

template <typename Traits>
class Heap {
    
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using Container         = typename Traits::Container;
    using Compare           = typename Traits::Compare;
    using ForwardIterator   = typename Container::ForwardIterator;
private:
    Container m_data;
    mutable mutex m_mutex;
    
public:
    ForwardIterator begin() { return m_data.begin(); }
    ForwardIterator end()   { return m_data.end();   }
    
    ostream &write(ostream &os) {
        lock_guard<mutex> lock(m_mutex);
        if (m_data.empty()) {
            return os << "[]";
        }
        return m_data.write(os);
    }

    friend ostream &operator <<(ostream &os, Heap<Traits> &heap) {
        return heap.write(os);
    }

    void push(const value_type &value, Ref ref) {
        lock_guard<mutex> lock(m_mutex);
        m_data.push_back(value, ref);
        HeapifyUp(m_data.size() - 1);
    }
private:
    bool win(size_t a, size_t b) {
        return Compare()(m_data[a].getValue(), m_data[b].getValue());
    }

    void HeapifyUp(size_t index) {
        while (index > 0) {
            size_t parentIndex = (index - 1) / 2;
            if (win(index, parentIndex)) {
                swap(m_data[index], m_data[parentIndex]);
                index = parentIndex;
            } else {
                break;
            }
        }
    }

    void HeapifyDown(size_t index) {
        size_t N = m_data.size();
        while (true) {
            size_t L = 2 * index + 1;
            size_t R = 2 * index + 2;
            size_t best = index;
            if (L < N && win(L, best)) {
                best = L;
            }
            if (R < N && win(R, best)) {
                best = R;
            }
            if (best == index) {
                break;
            }
            swap(m_data[index], m_data[best]);
            index = best;
        }
    }
public:
    Node pop() {
        lock_guard<mutex> lock(m_mutex);
        if (m_data.empty()) {
            throw out_of_range("El Heap esta vacio");
        }
        Node source = m_data[0];
        swap(m_data[0], m_data[m_data.size() - 1]);
        m_data.pop_back();
        if (!m_data.empty()) {
            HeapifyDown(0);
        }
        return source;
    }
    Node top() {
        lock_guard<mutex> lock(m_mutex);
        if (m_data.empty()) {
            throw out_of_range("El Heap esta vacio");
        }
        return m_data[0];
    }

    size_t size() const {
        lock_guard<mutex> lock(m_mutex);
        return m_data.size();
    }
    bool empty() const {
        lock_guard<mutex> lock(m_mutex);
        return m_data.empty();
    }
};

template <typename Traits>
class MinHeap : public Heap<MinHeapTraits<Traits>> {};

template <typename Traits>
class MaxHeap : public Heap<MaxHeapTraits<Traits>> {};



#endif // __HEAP_H__