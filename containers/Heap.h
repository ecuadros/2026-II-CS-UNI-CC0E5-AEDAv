#ifndef __HEAP_H__
#define __HEAP_H__

#include "traversalops.h"
#include "../types.h"
#include <cstddef>
#include <initializer_list>
#include <mutex>
#include <stdexcept>
#include <utility>

/**
 * Heap binario guardado en un arreglo; la raíz es el elemento que el comparador pone primero.
 * @tparam Traits Tipos de valor, nodo, comparador y almacenamiento.
 * @tparam Ops Mixins de recorrido, impresión y lectura.
 */
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

    /** Indica si el elemento en @p a debe estar por encima del elemento en @p b. */
    bool before(size_t a, size_t b) const { return m_comp(m_data[a].getValue(), m_data[b].getValue()); }

    /** Sube el elemento en @p index hasta restaurar la propiedad de heap. */
    void HeapifyUp(size_t index) {
        while (index > 0) {
            const auto parent = (index - 1) / 2;
            if (!before(index, parent)) break;
            std::swap(m_data[index], m_data[parent]);
            index = parent;
        }
    }

    /** Baja el elemento en @p index hasta restaurar la propiedad de heap. */
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

    /**
     * Comprueba que haya elementos.
     * @throws std::out_of_range si el heap está vacío.
     */
    void ensure_not_empty() const {
        if (m_data.empty()) throw std::out_of_range("Heap vacio");
    }

public:
    /** Crea un heap vacío. */
    Heap() = default;

    /** Crea un heap insertando los pares (valor, ref) dados. */
    Heap(std::initializer_list<std::pair<value_type, Ref>> values) { fill_from(*this, values); }

    /** Vista del heap como árbol para visualization.h; los nodos se identifican por su índice. */
    struct View {
        using Handle = size_t;
        const Storage* data;

        /** Índice de la raíz. */
        Handle root() const { return 0; }
        /** Índice del hijo izquierdo (@p side falso) o derecho (@p side verdadero). */
        Handle child(Handle i, bool side) const { return 2 * i + 1 + side; }
        /** Indica si el índice corresponde a un elemento. */
        bool valid(Handle i) const { return i < data->size(); }
        /** Escribe el texto del nodo. */
        void label(std::ostream& os, Handle i) const { os << (*data)[i]; }
    };

    /** Devuelve la vista de árbol que usa `print_tree`. */
    View tree_view() const { return View{&m_data}; }

    /** Iterador al primer elemento en el orden del arreglo, que no es el orden de prioridad. */
    ForwardIterator begin() const { return m_data.begin(); }
    /** Iterador de fin. */
    ForwardIterator end() const { return m_data.end(); }
    /** Iterador inverso al último elemento del arreglo. */
    BackwardIterator rbegin() const { return m_data.rbegin(); }
    /** Iterador inverso de fin. */
    BackwardIterator rend() const { return m_data.rend(); }

    /** Inserta un elemento en O(log n). */
    void push(const value_type& value, Ref ref) {
        std::lock_guard lock(this->m_mutex);
        m_data.push_back(value, ref);
        HeapifyUp(m_data.size() - 1);
    }

    /** Equivale a push(); existe para poder leer el heap desde un stream. */
    void push_back(const value_type& value, Ref ref) { push(value, ref); }

    /**
     * Quita y devuelve la raíz.
     * @throws std::out_of_range si el heap está vacío.
     */
    Node pop() {
        std::lock_guard lock(this->m_mutex);
        ensure_not_empty();
        Node root = m_data[0];
        std::swap(m_data[0], m_data[m_data.size() - 1]);
        m_data.pop_back();
        if (!m_data.empty()) HeapifyDown(0);
        return root;
    }

    /**
     * Devuelve la raíz sin quitarla.
     * @throws std::out_of_range si el heap está vacío.
     */
    Node top() const {
        std::lock_guard lock(this->m_mutex);
        ensure_not_empty();
        return m_data[0];
    }

    /** Indica si no hay elementos. */
    bool empty() const {
        std::lock_guard lock(this->m_mutex);
        return m_data.empty();
    }

    /** Cantidad de elementos. */
    size_t size() const {
        std::lock_guard lock(this->m_mutex);
        return m_data.size();
    }

    /** Elimina todos los elementos. */
    void clear() {
        std::lock_guard lock(this->m_mutex);
        m_data.clear();
    }
};

#endif // __HEAP_H__
