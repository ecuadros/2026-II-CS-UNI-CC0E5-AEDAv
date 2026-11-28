#ifndef __ARRAY_H__
#define __ARRAY_H__
#include <algorithm>
#include <compare>
#include <concepts>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "../core/GeneralIterator.h"
#include "../../types.h"

/**
 * Iterador de acceso aleatorio de Array sobre memoria contigua.
 * @tparam T Tipo de elemento; si es constante, el iterador no permite modificarlo.
 */
template <typename T>
class ArrayIterator : public GeneralIterator<ArrayIterator<T>, T> {
public:
    using MySelf            = ArrayIterator<T>;
    using Parent            = GeneralIterator<MySelf, T>;
    using iterator_category = std::random_access_iterator_tag;
    using iterator_concept  = std::random_access_iterator_tag;
    using difference_type   = Offset;
    using value_type        = std::remove_const_t<T>;

    /** Crea el iterador apuntando a @p ptr; sin argumento es un iterador nulo. */
    ArrayIterator(T* ptr = nullptr) : Parent(ptr) {}

    ArrayIterator& operator++() { ++this->m_ptr; return *this; }
    ArrayIterator& operator--() { --this->m_ptr; return *this; }
    ArrayIterator operator++(Int) { MySelf previous = *this; ++*this; return previous; }
    ArrayIterator operator--(Int) { MySelf previous = *this; --*this; return previous; }

    ArrayIterator& operator+=(Offset n) { this->m_ptr += n; return *this; }
    ArrayIterator& operator-=(Offset n) { this->m_ptr -= n; return *this; }
    friend ArrayIterator operator+(MySelf it, Offset n) { return it += n; }
    friend ArrayIterator operator+(Offset n, MySelf it) { return it += n; }
    friend ArrayIterator operator-(MySelf it, Offset n) { return it -= n; }
    friend Offset operator-(const MySelf& a, const MySelf& b) { return a.m_ptr - b.m_ptr; }

    /** Elemento a @p n posiciones del actual. */
    T& operator[](Offset n) const { return this->m_ptr[n]; }
    friend std::strong_ordering operator<=>(const MySelf& a, const MySelf& b) { return a.m_ptr <=> b.m_ptr; }
};

/** Rasgos válidos para Array: valor e iteradores, sin nodo. */
template <typename Traits>
concept ArrayTraitsConcept = requires {
    typename Traits::value_type;
    typename Traits::ForwardIterator;
    typename Traits::ConstForwardIterator;
};

/**
 * Tipos que configuran un Array: valor e iteradores.
 * @tparam T Tipo del valor.
 */
template <typename T> struct ArrayTraits {
    using value_type = T;
    using ForwardIterator = ArrayIterator<T>;
    using ConstForwardIterator = ArrayIterator<const T>;
};

/**
 * Arreglo dinámico de elementos guardados tal cual, sin nodo ni ref. A diferencia de Vector no toma mutex:
 * sirve como almacenamiento interno de un contenedor que ya se sincroniza (por ejemplo, las páginas de BTree).
 * @tparam Traits Tipo de valor e iteradores.
 */
template <typename Traits>
    requires ArrayTraitsConcept<Traits>
class Array {
public:
    using value_type            = Traits::value_type;
    using ForwardIterator       = Traits::ForwardIterator;
    using ConstForwardIterator  = Traits::ConstForwardIterator;
private:
    value_type  *m_data     = nullptr;   // puntero al arreglo dinámico
    Count        m_size     = 0,         // cantidad actual
                 m_capacity = 0;         // capacidad

    template <typename Self>
    using IteratorFor = std::conditional_t<std::is_const_v<Self>, ConstForwardIterator, ForwardIterator>;

    /** Amplía la capacidad a @p new_cap moviendo los elementos; no hace nada si ya es suficiente. */
    void resize(Count new_cap) {
        if (new_cap <= m_capacity) return;
        auto new_data = std::make_unique<value_type[]>(new_cap);
        for (Count i = 0; i < m_size; ++i)
            new_data[i] = std::move_if_noexcept(m_data[i]);
        delete[] m_data;
        m_data = new_data.release();
        m_capacity = new_cap;
    }

    template <typename V>
    void push_back_impl(V&& value) {
        if (m_size == m_capacity)
            resize(m_capacity == 0 ? 10 : m_capacity * 2);
        m_data[m_size++] = std::forward<V>(value);
    }

public:
    /** Crea un arreglo vacío. */
    Array() = default;

    /** Crea un arreglo con @p size elementos inicializados por defecto. */
    explicit Array(Count size) {
        resize(size);
        m_size = size;
    }

    /** Crea un arreglo con los elementos de @p values. */
    Array(std::initializer_list<value_type> values) {
        resize(static_cast<Count>(values.size()));
        for (const value_type& value : values)
            m_data[m_size++] = value;
    }

    /** Libera la memoria. */
    ~Array() { delete[] m_data; }

    /** Copia los elementos de @p other con la capacidad ajustada a su tamaño. */
    Array(const Array& other)
        requires std::copyable<value_type>
    {
        if (other.m_size == 0) return;
        auto data = std::make_unique<value_type[]>(other.m_size);
        for (Count i = 0; i < other.m_size; ++i)
            data[i] = other.m_data[i];
        m_data = data.release();
        m_size = m_capacity = other.m_size;
    }

    /** Mueve el contenido de @p other, que queda vacío. */
    Array(Array&& other) noexcept { swap(other); }

    /** Asigna por copia o por movimiento (copy-and-swap). */
    Array& operator=(Array other) noexcept {
        swap(other);
        return *this;
    }

    /** Intercambia el contenido con @p other. */
    void swap(Array& other) noexcept {
        std::swap(m_data, other.m_data);
        std::swap(m_size, other.m_size);
        std::swap(m_capacity, other.m_capacity);
    }

    /** Añade un elemento al final; si no hay espacio duplica la capacidad (10 la primera vez). */
    void push_back(const value_type& value) { push_back_impl(value); }

    /** Añade un elemento al final moviendo @p value. */
    void push_back(value_type&& value) { push_back_impl(std::move(value)); }

    /**
     * Devuelve el elemento en la posición @p index; constante si el arreglo lo es.
     * @throws std::out_of_range si @p index está fuera de rango.
     */
    template <typename Self>
    [[nodiscard]] auto& at(this Self& self, Count index) {
        if (index >= self.m_size) throw std::out_of_range("Indice fuera de rango");
        return self.m_data[index];
    }

    /** Equivale a at(): también comprueba el rango. */
    template <typename Self>
    [[nodiscard]] auto& operator[](this Self& self, Count index) { return self.at(index); }

    /**
     * Devuelve el último elemento.
     * @throws std::out_of_range si el arreglo está vacío.
     */
    template <typename Self>
    [[nodiscard]] auto& back(this Self& self) { return self.at(self.m_size - 1); }

    /** Cantidad de elementos. */
    [[nodiscard]] Count size()  const { return m_size; }
    /** Indica si no hay elementos. */
    [[nodiscard]] Bool  empty() const { return m_size == 0; }

    /** Indica si ambos arreglos tienen los mismos elementos en el mismo orden. */
    friend Bool operator==(const Array& a, const Array& b)
        requires std::equality_comparable<value_type>
    {
        return a.m_size == b.m_size && std::equal(a.m_data, a.m_data + a.m_size, b.m_data);
    }

    /** Iterador al primer elemento; es constante si el arreglo lo es. */
    template <typename Self> auto begin(this Self& self) { return IteratorFor<Self>(self.m_data); }
    /** Iterador a la posición siguiente al último elemento. */
    template <typename Self> auto end(this Self& self) { return IteratorFor<Self>(self.m_data + self.m_size); }
};

#endif // __ARRAY_H__
