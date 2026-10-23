#ifndef __HEAP_H__
#define __HEAP_H__
#include <stdexcept>
#include "vector.h"
#include "GeneralContainer.h"

template <typename T, typename _Compare>
struct HeapTraits : public DefaultTraits<T, _Compare> {
    using Node            = GeneralNode<T>;
    using Container       = Vector<VectorAscTraits<T>>; 
    using ForwardIterator = typename Container::ForwardIterator;
};

template <typename T>
struct MinHeapTraits : public HeapTraits<T, std::less<T>> {};

template <typename T>
struct MaxHeapTraits : public HeapTraits<T, std::greater<T>> {};

template <typename Traits>
class Heap : public GeneralContainer<Traits> {
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using Compare           = typename Traits::Compare;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using Container         = typename Traits::Container;
    using Base              = GeneralContainer<Traits>;
    using Base::m_mutex;
private:
    Container m_data;

    bool win(size_t a, size_t b) {
        return Compare()(m_data[a].getValue(), m_data[b].getValue());
    }

    void HeapifyUp(size_t index) {
        while (index > 0) {
            size_t parentIndex = (index - 1) / 2;
            if (!win(index, parentIndex))
                break;
            swap(m_data[index], m_data[parentIndex]);
            index = parentIndex;
        }
    }

    void HeapifyDown(size_t index) {
        size_t N = m_data.size();
        while (true) {
            size_t L = 2 * index + 1;
            size_t R = 2 * index + 2;
            size_t best = index;
            if (L < N && win(L, best)) best = L;
            if (R < N && win(R, best)) best = R;
            if (best == index)
                break;
            swap(m_data[index], m_data[best]);
            index = best;
        }
    }

public:
    ForwardIterator begin() const override { return m_data.begin(); }
    ForwardIterator end()   const override { return m_data.end();   }

    void push_back(const value_type &value, Ref ref) override {
        lock_guard<mutex> lock(m_mutex);
        m_data.push_back(value, ref);
        HeapifyUp(m_data.size() - 1);
    }

    void clear() override {
        lock_guard<mutex> lock(m_mutex);
        m_data.clear();
    }

    Node pop() {
        lock_guard<mutex> lock(m_mutex);
        if (m_data.empty())
            throw out_of_range("El Heap esta vacio");
        Node source = m_data[0];
        swap(m_data[0], m_data[m_data.size() - 1]);
        m_data.pop_back();
        if (!m_data.empty())
            HeapifyDown(0);
        return source;
    }

    Node top() {
        lock_guard<mutex> lock(m_mutex);
        if (m_data.empty())
            throw out_of_range("El Heap esta vacio");
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

template <typename T>
class MinHeap : public Heap<MinHeapTraits<T>> {};

template <typename T>
class MaxHeap : public Heap<MaxHeapTraits<T>> {};

#endif // __HEAP_H__