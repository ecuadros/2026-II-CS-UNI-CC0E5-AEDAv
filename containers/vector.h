#ifndef __VECTOR_H__
#define __VECTOR_H__
#include <cstddef>
#include <initializer_list>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "GeneralIterator.h"
#include "traversalops.h"
#include "traits.h"
#include "../types.h" // Ref

// Reverse = false avanza con ++, Reverse = true retrocede con --
template <typename T, bool Reverse>
class VectorIterator : public GeneralIterator<VectorIterator<T, Reverse>, T> {
public:
    using value_type        = T;
    using MySelf            = VectorIterator<T, Reverse>;
    using Parent            = GeneralIterator<MySelf, T>;
    using Parent::Parent; // Inherit constructor

    // Prefix increment
    VectorIterator& operator++() {
        if constexpr (Reverse) --Parent::m_ptr;
        else                   ++Parent::m_ptr;
        return *this;
    }
};

template <typename Traits>
class Vector : public FullTraversalOps {
public:
    using value_type        = Traits::value_type;
    using Node              = Traits::Node;
    using ForwardIterator   = Traits::ForwardIterator;
    using BackwardIterator  = Traits::BackwardIterator;
    using ConstForwardIterator  = VectorIterator<const Node, false>;
    using ConstBackwardIterator = VectorIterator<const Node, true>;
private:
    Node        *m_data     = nullptr;   // puntero al arreglo dinámico
    size_t       m_size     = 0,         // cantidad actual
                 m_capacity = 0;         // capacidad

    // Iterador const o no const segun la constness de Self
    template <typename Self, bool Reverse>
    using IteratorFor = VectorIterator<std::conditional_t<std::is_const_v<Self>, const Node, Node>, Reverse>;

    void resize(size_t new_cap) {
        if (new_cap <= m_capacity) return;
        Node* new_data = new Node[new_cap];
        for (size_t i = 0; i < m_size; ++i)
            new_data[i] = m_data[i];
        delete[] m_data;
        m_data = new_data;
        m_capacity = new_cap;
    }

public:
    Vector() {}

    // Cada elemento de la lista es una pareja (valor, ref) para un Node
    Vector(std::initializer_list<std::pair<value_type, Ref>> values) { fill_from(*this, values); }

    virtual ~Vector() {
        clear();
    }

    Vector(const Vector& other) {
        std::lock_guard lock(other.m_mutex);
        resize(other.m_size);
        for (size_t i = 0; i < other.m_size; ++i)
            m_data[i] = other.m_data[i];
        m_size = other.m_size;
    }

    Vector(Vector&& other) noexcept { swap(other); }

    // Copy-and-swap: sirve para copia y movimiento; other libera el estado viejo
    Vector& operator=(Vector other) noexcept {
        swap(other);
        return *this;
    }

    void swap(Vector& other) noexcept {
        if (this == &other) return;
        std::scoped_lock lock(m_mutex, other.m_mutex);
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    void push_back(const value_type& value, Ref ref) {
        std::lock_guard lock(m_mutex);
        if (m_size == m_capacity) {
            size_t new_cap = (m_capacity == 0) ? 10 : m_capacity * 2;
            resize(new_cap);
        }
        m_data[m_size] = Node(value, ref);
        ++m_size;
    }

    void pop_back() {
        std::lock_guard lock(m_mutex);
        if (m_size > 0)
            --m_size;
    }

    // Una sola version para const y no const (deducing this)
    template <typename Self>
    auto& at(this Self& self, size_t index) {
        if (index >= self.m_size) throw std::out_of_range("Indice fuera de rango");
        return self.m_data[index];
    }

    template <typename Self>
    auto& operator[](this Self& self, size_t index) { return self.at(index); }

    size_t size()     const { std::lock_guard lock(m_mutex); return m_size; }
    size_t capacity() const { std::lock_guard lock(m_mutex); return m_capacity; }
    bool   empty()    const { std::lock_guard lock(m_mutex); return m_size == 0; }

    void clear() {
        std::lock_guard lock(m_mutex);
        delete [] m_data;
        m_data     = nullptr;
        m_size     = 0;
        m_capacity = 0;
    }

    template <typename Self> auto begin(this Self& self) {
        return IteratorFor<Self, false>(self.m_data);
    }
    template <typename Self> auto end(this Self& self) {
        return IteratorFor<Self, false>(self.m_data + self.m_size);
    }
    template <typename Self> auto rbegin(this Self& self) {
        return IteratorFor<Self, true>(self.m_data + self.m_size - 1);
    }
    template <typename Self> auto rend(this Self& self) {
        return IteratorFor<Self, true>(self.m_data - 1);
    }
};

#endif // __VECTOR_H__
