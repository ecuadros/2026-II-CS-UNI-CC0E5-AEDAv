#ifndef __HEAP_H__
#define __HEAP_H__

#include "traversalops.h"
#include "../types.h"
#include <cstddef>
#include <initializer_list>
#include <mutex>
#include <stdexcept>
#include <utility>

template <typename Traits, typename Ops = SearchTraversalOps> class Heap : public Ops {

public:
    using value_type = typename Traits::value_type;
    using Node = typename Traits::Node;
    using Compare = typename Traits::Compare;
    using Storage = typename Traits::Storage;
    using ForwardIterator = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;

private:
    Storage m_data;
    Compare m_comp;

    bool before(size_t a, size_t b) const { return m_comp(m_data[a].getValue(), m_data[b].getValue()); }

    void HeapifyUp(size_t index) {
        while (index > 0) {
            const auto parent = (index - 1) / 2;
            if (!before(index, parent)) break;
            std::swap(m_data[index], m_data[parent]);
            index = parent;
        }
    }

    void HeapifyDown(size_t index) {
        const auto size = m_data.size();
        while (index < size / 2) {
            const auto left = 2 * index + 1;
            const auto right = left + 1;
            auto best = right < size && before(right, left) ? right : left;
            if (!before(best, index)) break;
            std::swap(m_data[index], m_data[best]);
            index = best;
        }
    }

    void ensure_not_empty() const {
        if (m_data.empty()) throw std::out_of_range("Heap vacio");
    }

public:
    Heap() = default;

    Heap(std::initializer_list<std::pair<value_type, Ref>> values) { fill_from(*this, values); }

    struct View {
        using Handle = size_t;
        const Storage* data;

        Handle root() const { return 0; }
        Handle child(Handle i, bool side) const { return 2 * i + 1 + side; }
        bool valid(Handle i) const { return i < data->size(); }
        void label(std::ostream& os, Handle i) const { os << (*data)[i]; }
    };

    View tree_view() const { return View{&m_data}; }

    ForwardIterator begin() const { return m_data.begin(); }
    ForwardIterator end() const { return m_data.end(); }
    BackwardIterator rbegin() const { return m_data.rbegin(); }
    BackwardIterator rend() const { return m_data.rend(); }

    void push(const value_type& value, Ref ref) {
        std::lock_guard lock(this->m_mutex);
        m_data.push_back(value, ref);
        HeapifyUp(m_data.size() - 1);
    }

    void push_back(const value_type& value, Ref ref) { push(value, ref); }

    Node pop() {
        std::lock_guard lock(this->m_mutex);
        ensure_not_empty();
        Node root = m_data[0];
        std::swap(m_data[0], m_data[m_data.size() - 1]);
        m_data.pop_back();
        if (!m_data.empty()) HeapifyDown(0);
        return root;
    }

    Node top() const {
        std::lock_guard lock(this->m_mutex);
        ensure_not_empty();
        return m_data[0];
    }

    bool empty() const {
        std::lock_guard lock(this->m_mutex);
        return m_data.empty();
    }

    size_t size() const {
        std::lock_guard lock(this->m_mutex);
        return m_data.size();
    }

    void clear() {
        std::lock_guard lock(this->m_mutex);
        m_data.clear();
    }
};

#endif // __HEAP_H__
