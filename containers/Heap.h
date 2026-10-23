#ifndef __HEAP_H__
#define __HEAP_H__

#include "traits.h"
#include "container_stream.h"
#include "traversalops.h"
#include <cstddef>
#include <initializer_list>
#include <mutex>

template <typename Traits> class Heap : public SearchTraversalOps<Heap<Traits>> {
    friend class CallOp<Heap>;
    friend class FirstThatOp<Heap>;

public:
    using container_stream_tag = void;
    using traits_type = Traits;
    using value_type = typename Traits::value_type;
    using Node = typename Traits::Node;
    using NodePtr = Node*;
    using Compare = typename Traits::compare_type;
    using Storage = typename Traits::Storage;
    using ForwardIterator = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;

private:
    Storage m_data;
    mutable std::mutex m_mutex;

    void HeapifyUp(size_t index) {
        Compare compare;
        while (index > 0) {
            size_t parentIndex = (index - 1) / 2;
            if (!compare(m_data[index].getValue(), m_data[parentIndex].getValue())) break;
            std::swap(m_data[index], m_data[parentIndex]);
            index = parentIndex;
        }
    }

    void HeapifyDown(size_t index) {
        Compare compare;
        const size_t n = m_data.size();
        while (index < n && index < n / 2) {
            const size_t left = 2 * index + 1;
            const size_t right = left + 1;
            size_t best = index;
            if (left < n && compare(m_data[left].getValue(), m_data[best].getValue())) best = left;
            if (right < n && compare(m_data[right].getValue(), m_data[best].getValue())) best = right;
            if (best == index) break;
            std::swap(m_data[index], m_data[best]);
            index = best;
        }
    }

public:
    Heap() = default;

    Heap(std::initializer_list<std::pair<value_type, Ref>> initList) {
        for (const auto& pair : initList) {
            push(pair.first, pair.second);
        }
    }

    ForwardIterator begin() const { return m_data.begin(); }
    ForwardIterator end() const { return m_data.end(); }
    BackwardIterator rbegin() const { return m_data.rbegin(); }
    BackwardIterator rend() const { return m_data.rend(); }

    std::ostream& write(std::ostream& os) const {
        std::lock_guard lock(m_mutex);
        os << '[';
        const auto count = m_data.size();
        for (std::size_t i = 0; i < count; ++i) {
            if (i != 0) os << ',';
            os << m_data[i];
        }
        return os << ']';
    }

    std::istream& read(std::istream& is) {
        Heap<Traits> parsed;
        if (!container_stream::read(is, parsed)) return is;
        std::lock_guard lock(m_mutex);
        m_data.swap(parsed.m_data);
        return is;
    }

    void push(const value_type& value, Ref ref) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.push_back(value, ref);
        HeapifyUp(m_data.size() - 1);
    }

    void push_back(const value_type& value, Ref ref) { push(value, ref); }

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

#endif // __HEAP_H__
