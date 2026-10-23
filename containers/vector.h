#ifndef __VECTOR_H__
#define __VECTOR_H__
#include <mutex>
#include <initializer_list>
#include "GeneralIterator.h"
#include "GeneralNode.h"
#include "container_stream.h"
#include "traversalops.h"
#include "../foreach.h"
#include "traits.h"
#include "../types.h" // Ref
using namespace std;

template <typename T>
class VectorForwardIterator : public GeneralIterator<VectorForwardIterator<T>, T> {
public:
    using value_type        = T;
    using MySelf            = VectorForwardIterator<T>;
    using Parent            = GeneralIterator<MySelf, T>;
    using Parent::Parent; // Inherit constructor

    // Prefix increment
    T* operator->() const { return Parent::m_ptr; }
    VectorForwardIterator& operator++() { ++Parent::m_ptr; return *this; }
    VectorForwardIterator operator++(int) {
        auto previous = *this;
        ++*this;
        return previous;
    }
};

template <typename T>
class VectorBackwardIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type = std::ptrdiff_t;
    using value_type = T;
    using pointer = T*;
    using reference = T&;
    explicit VectorBackwardIterator(pointer base) : m_base(base) {}
    reference operator*() const { return *(m_base - 1); }
    pointer operator->() const { return m_base - 1; }
    VectorBackwardIterator& operator++() { --m_base; return *this; }
    VectorBackwardIterator operator++(int) {
        auto previous = *this;
        ++*this;
        return previous;
    }
    friend bool operator==(const VectorBackwardIterator& a, const VectorBackwardIterator& b) {
        return a.m_base == b.m_base;
    }
private:
    pointer m_base;
};

template <typename T>
struct VectorAscTraits {
    using value_type        = T;
    using Node              = GeneralNode<T>;
    using ForwardIterator   = VectorForwardIterator<Node>;  // itera sobre Node, no sobre T
    using BackwardIterator  = VectorBackwardIterator<Node>; // itera sobre Node, no sobre T
};

template <typename Traits>
class Vector : public FullTraversalOps<Vector<Traits>> {
    friend class CallOp<Vector>;
    friend class FirstThatOp<Vector>;
    friend class ApplyFunctionOp<Vector>;
public:
    using container_stream_tag = void;
    using value_type        = Traits::value_type;
    using Node              = Traits::Node;
    using ForwardIterator   = Traits::ForwardIterator;
    using BackwardIterator  = Traits::BackwardIterator;
    using ConstForwardIterator = VectorForwardIterator<const Node>;
    using ConstBackwardIterator = VectorBackwardIterator<const Node>;
private:
    Node        *m_data     = nullptr;   // puntero al arreglo dinámico
    size_t       m_size     = 0,         // cantidad actual
                 m_capacity = 0;         // capacidad
    mutable mutex m_mutex;               // mutex para sincronización

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
    Vector(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            push_back(v.first, v.second);
    }

    virtual ~Vector() {
        clear();
    }

    Vector(const Vector& other) {
        lock_guard<mutex> lock(other.m_mutex);
        resize(other.m_size);
        for (size_t i = 0; i < other.m_size; ++i)
            m_data[i] = other.m_data[i];
        m_size = other.m_size;
    }

    // Move constructor and move assignment operator
    Vector(Vector&& other) noexcept {
        lock_guard<mutex> lock(other.m_mutex);
        m_data     = std::exchange(other.m_data, nullptr);
        m_size     = std::exchange(other.m_size, 0);
        m_capacity = std::exchange(other.m_capacity, 0);
    }

    // Move assignment operator
    Vector& operator=(Vector&& other) noexcept {
        if (this != &other) {
            scoped_lock lock(m_mutex, other.m_mutex);
            delete[] m_data;
            m_data     = std::exchange(other.m_data, nullptr);
            m_size     = std::exchange(other.m_size, 0);
            m_capacity = std::exchange(other.m_capacity, 0);
        }
        return *this;
    }

    Vector& operator=(const Vector& other) {
        if (this == &other) return *this;
        Vector copy(other);
        swap(copy);
        return *this;
    }

    void swap(Vector& other) noexcept {
        if (this == &other) return;
        scoped_lock lock(m_mutex, other.m_mutex);
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    void push_back(const value_type& value, Ref ref) {
        lock_guard<mutex> lock(m_mutex);
        if (m_size == m_capacity) {
            size_t new_cap = (m_capacity == 0) ? 10 : m_capacity * 2;
            resize(new_cap);
        }
        m_data[m_size] = Node(value, ref);
        ++m_size;
    }

    void pop_back() {
        lock_guard<mutex> lock(m_mutex);
        if (m_size > 0)
            --m_size;
    }

    Node& operator[](size_t index) {
        if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
        return m_data[index];
    }

    const Node& operator[](size_t index) const {
        if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
        return m_data[index];
    }

    Node& at(size_t index) {
        if (index >= m_size) throw std::out_of_range("Indice fuera de rango");
        return m_data[index];
    }

    size_t size()     const { lock_guard<mutex> lock(m_mutex); return m_size; }
    size_t capacity() const { lock_guard<mutex> lock(m_mutex); return m_capacity; }
    bool   empty()    const { lock_guard<mutex> lock(m_mutex); return m_size == 0; }

    void clear() {
        lock_guard<mutex> lock(m_mutex);
        delete [] m_data;
        m_data     = nullptr;
        m_size     = 0;
        m_capacity = 0;
    }

    ForwardIterator begin() { return ForwardIterator(m_data); }
    ForwardIterator end() { return ForwardIterator(m_data ? m_data + m_size : nullptr); }
    BackwardIterator rbegin() { return BackwardIterator(m_data ? m_data + m_size : nullptr); }
    BackwardIterator rend() { return BackwardIterator(m_data); }

    ConstForwardIterator begin() const { return ConstForwardIterator(m_data); }
    ConstForwardIterator end() const {
        return ConstForwardIterator(m_data ? m_data + m_size : nullptr);
    }
    ConstBackwardIterator rbegin() const {
        return ConstBackwardIterator(m_data ? m_data + m_size : nullptr);
    }
    ConstBackwardIterator rend() const { return ConstBackwardIterator(m_data); }

    // Persistencia
    ostream &write(ostream &os) const {
        lock_guard<mutex> lock(m_mutex);
        return container_stream::write(os, begin(), end());
    }

    // TODO: implementar la lectura de un vector desde un stream
    istream &read(istream &is) {
        Vector parsed;
        if (!container_stream::read(is, parsed)) return is;
        swap(parsed);
        return is;
    }
};

#endif // __VECTOR_H__
