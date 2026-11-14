#ifndef __GENERAL_ITERATOR_H__
#define __GENERAL_ITERATOR_H__

#include <cstddef>
#include <iterator>

// CRTP
/**
 * Base CRTP de los iteradores: guarda el puntero actual y aporta `*`, `->` y `==`.
 * @tparam Derived Iterador concreto, que debe definir `operator++`.
 * @tparam T Tipo apuntado (nodo o elemento).
 */
template <typename Derived, typename T>
class GeneralIterator {
public:
    using iterator_category = std::forward_iterator_tag;
    using difference_type   = std::ptrdiff_t;
    using value_type        = T;
    using pointer           = value_type *;
    using reference         = value_type&;
protected:
    /** Elemento actual. */
    pointer m_ptr;
    /** Elemento inicial; las listas circulares lo usan para detectar la vuelta completa. */
    pointer m_pStart;
public:
    /** Crea el iterador apuntando a @p ptr, que también queda como punto de inicio. */
    GeneralIterator(pointer ptr) : m_ptr(ptr), m_pStart(ptr) {}
    /** Devuelve el elemento actual. */
    value_type& operator*() const { return *m_ptr; }
    /** Accede a los miembros del elemento actual. */
    pointer operator->() const { return m_ptr; }
    /** Dos iteradores son iguales si apuntan al mismo elemento. */
    friend bool operator==(const Derived& a, const Derived& b) { return a.m_ptr == b.m_ptr; }
};

#endif // __GENERAL_ITERATOR_H__
