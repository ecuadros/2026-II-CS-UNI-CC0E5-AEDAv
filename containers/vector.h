#ifndef __VECTOR_H__
#define __VECTOR_H__
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "GeneralIterator.h"
#include "traversalops.h"
#include "traits.h"
#include "../types.h" // Ref

/**
 * Iterador de Vector sobre memoria contigua.
 * @tparam T Tipo de elemento.
 * @tparam Reverse Si es `true`, avanza hacia posiciones menores.
 */
template <typename T, bool Reverse>
class VectorIterator : public GeneralIterator<VectorIterator<T, Reverse>, T> {
public:
    using value_type        = T;
    using MySelf            = VectorIterator<T, Reverse>;
    using Parent            = GeneralIterator<MySelf, T>;
    using Parent::Parent; // Inherit constructor

    // Prefix increment
    /** Avanza al elemento siguiente en el sentido del iterador. */
    VectorIterator& operator++() {
        if constexpr (Reverse) --Parent::m_ptr;
        else                   ++Parent::m_ptr;
        return *this;
    }
};

/**
 * Arreglo dinámico de nodos (valor, ref). Las modificaciones y las consultas de tamaño toman el mutex del contenedor.
 * @tparam Traits Tipos de valor, nodo e iteradores.
 * @tparam Ops Mixins de recorrido, impresión y lectura.
 */
template <typename Traits, typename Ops>
class Vector : public Ops {
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

    template <typename Self, bool Reverse>
    using IteratorFor = VectorIterator<std::conditional_t<std::is_const_v<Self>, const Node, Node>, Reverse>;

    /** Amplía la capacidad a @p new_cap copiando los elementos; no hace nada si ya es suficiente. */
    void resize(size_t new_cap) {
        if (new_cap <= m_capacity) return;
        std::unique_ptr<Node[]> new_data(new Node[new_cap]);
        for (size_t i = 0; i < m_size; ++i)
            new_data[i] = m_data[i];
        delete[] m_data;
        m_data = new_data.release();
        m_capacity = new_cap;
    }

public:
    /** Crea un vector vacío. */
    Vector() {}

    /** Crea un vector con los pares (valor, ref) dados. */
    Vector(std::initializer_list<std::pair<value_type, Ref>> values) { fill_from(*this, values); }

    /** Libera la memoria. */
    virtual ~Vector() {
        clear();
    }

    /** Copia los elementos de @p other con la capacidad ajustada a su tamaño. */
    Vector(const Vector& other) {
        std::lock_guard lock(other.m_mutex);
        if (other.m_size == 0) return;
        std::unique_ptr<Node[]> data(new Node[other.m_size]);
        for (size_t i = 0; i < other.m_size; ++i)
            data[i] = other.m_data[i];
        m_data = data.release();
        m_size = m_capacity = other.m_size;
    }

    /** Mueve el contenido de @p other, que queda vacío. */
    Vector(Vector&& other) noexcept { swap(other); }

    // Copy-and-swap: sirve para copia y movimiento; other libera el estado viejo
    /** Asigna por copia o por movimiento (copy-and-swap). */
    Vector& operator=(Vector other) noexcept {
        swap(other);
        return *this;
    }

    /** Intercambia el contenido con @p other. */
    void swap(Vector& other) noexcept {
        if (this == &other) return;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    /** Añade un elemento al final; si no hay espacio duplica la capacidad (10 la primera vez). */
    void push_back(const value_type& value, Ref ref) {
        std::lock_guard lock(this->m_mutex);
        Node node(value, ref);
        if (m_size == m_capacity) {
            size_t new_cap = (m_capacity == 0) ? 10 : m_capacity * 2;
            resize(new_cap);
        }
        m_data[m_size] = node;
        ++m_size;
    }

    /** Quita el último elemento; no hace nada si está vacío. */
    void pop_back() {
        std::lock_guard lock(this->m_mutex);
        if (m_size > 0)
            --m_size;
    }

    /**
     * Devuelve el nodo en la posición @p index.
     * @throws std::out_of_range si @p index está fuera de rango.
     */
    template <typename Self>
    auto& at(this Self& self, size_t index) {
        if (index >= self.m_size) throw std::out_of_range("Indice fuera de rango");
        return self.m_data[index];
    }

    /** Equivale a at(): también comprueba el rango. */
    template <typename Self>
    auto& operator[](this Self& self, size_t index) { return self.at(index); }

    /** Cantidad de elementos. */
    size_t size()     const { std::lock_guard lock(this->m_mutex); return m_size; }
    /** Cantidad de elementos que caben sin reservar más memoria. */
    size_t capacity() const { std::lock_guard lock(this->m_mutex); return m_capacity; }
    /** Indica si no hay elementos. */
    bool   empty()    const { std::lock_guard lock(this->m_mutex); return m_size == 0; }

    /** Elimina todos los elementos y libera la memoria. */
    void clear() {
        std::lock_guard lock(this->m_mutex);
        delete [] m_data;
        m_data     = nullptr;
        m_size     = 0;
        m_capacity = 0;
    }

    /** Iterador al primer elemento; es constante si el vector lo es. */
    template <typename Self> auto begin(this Self& self) {
        return IteratorFor<Self, false>(self.m_data);
    }
    /** Iterador a la posición siguiente al último elemento. */
    template <typename Self> auto end(this Self& self) {
        return IteratorFor<Self, false>(self.m_data + self.m_size);
    }
    /** Iterador inverso al último elemento. */
    template <typename Self> auto rbegin(this Self& self) {
        return IteratorFor<Self, true>(self.m_data + self.m_size - 1);
    }
    /** Iterador inverso a la posición anterior al primer elemento. */
    template <typename Self> auto rend(this Self& self) {
        return IteratorFor<Self, true>(self.m_data - 1);
    }
};

#endif // __VECTOR_H__
