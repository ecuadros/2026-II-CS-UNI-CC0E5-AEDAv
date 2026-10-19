#ifndef __HEAP_H__
#define __HEAP_H__

#include "traversalops.h"
#include "heaptraits.h"
#include "vector.h"
#include <initializer_list>
#include <mutex>

template <typename Traits> class Heap : public SearchTraversalOps<Heap<Traits>> {
    friend class CallOp<Heap>;
    friend class FirstThatOp<Heap>;

public:
    using value_type = typename Traits::value_type;
    using Node = typename Traits::Node;
    using NodePtr = Node*;
    using Compare = typename Traits::compare_type;

private:
    mutable Vector<VectorAscTraits<value_type>> m_data;
    mutable std::mutex m_mutex;

public:
    Heap() = default;

    Heap(std::initializer_list<std::pair<value_type, Ref>> initList) {
        for (const auto& pair : initList) {
            push(pair.first, pair.second);
        }
    }

    decltype(auto) begin() const { return m_data.begin(); }

    decltype(auto) end() const { return m_data.end(); }

    friend ostream& operator<<(ostream& os, const Heap<Traits>& heap) {
        std::lock_guard<std::mutex> lock(heap.m_mutex);
        os << "[";
        for (auto it = heap.begin(); it != heap.end(); ++it) {
            os << *it;
            if (std::next(it) != heap.end()) {
                os << ", ";
            }
        }
        return os << "]";
    }

    friend std::istream& operator>>(std::istream& is, Heap<Traits>& heap) {
        Heap<Traits> parsed;
        auto expect = [&is](char expected) {
            char actual;
            if (!(is >> actual)) return false;
            if (actual == expected) return true;
            is.setstate(std::ios::failbit);
            return false;
        };

        if (!expect('[')) return is;
        is >> std::ws;
        if (is.peek() == ']') {
            is.get();
        } else {
            while (true) {
                value_type value;
                Ref ref;
                if (!expect('(') || !(is >> value) || !expect(',') || !(is >> ref) ||
                    !expect(')'))
                    return is;

                parsed.push(value, ref);

                char separator;
                if (!(is >> separator)) return is;
                if (separator == ']') break;
                if (separator != ',') {
                    is.setstate(std::ios::failbit);
                    return is;
                }
            }
        }

        std::lock_guard<std::mutex> lock(heap.m_mutex);
        heap.m_data = std::move(parsed.m_data);
        return is;
    }

    void push(const value_type& value, Ref ref) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.push_back(value, ref);
        HeapifyUp(m_data.size() - 1);
    }

    void push_back(const value_type& value, Ref ref) { push(value, ref); }

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

    void HeapifyDown(size_t index) {
        size_t n = m_data.size();
        while (true) {
            size_t leftIndex = 2 * index + 1;
            size_t rightIndex = 2 * index + 2;
            size_t bestIndex = index;

            if (leftIndex < n &&
                Compare()(m_data[leftIndex].getValue(), m_data[bestIndex].getValue()))
                bestIndex = leftIndex;

            if (rightIndex < n &&
                Compare()(m_data[rightIndex].getValue(), m_data[bestIndex].getValue()))
                bestIndex = rightIndex;

            if (bestIndex != index) {
                std::swap(m_data[index], m_data[bestIndex]);
                index = bestIndex;
            } else {
                break;
            }
        }
    }

    Node pop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_data.empty()) throw std::out_of_range("Heap vacio");
        Node root = m_data[0];
        std::swap(m_data[0], m_data[m_data.size() - 1]);
        m_data.pop_back();
        if (!m_data.empty()) {
            HeapifyDown(0);
        }
        return root;
    }

    Node top() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_data.empty()) throw std::out_of_range("Heap vacio");
        return m_data[0];
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_data.empty();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_data.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.clear();
    }
};

template <typename T> class MinHeap : public Heap<MinHeapTraits<T>> {
public:
    using Parent = Heap<MinHeapTraits<T>>;
    using Parent::Parent;
};

template <typename T> class MaxHeap : public Heap<MaxHeapTraits<T>> {
public:
    using Parent = Heap<MaxHeapTraits<T>>;
    using Parent::Parent;
};

#endif // __HEAP_H__
