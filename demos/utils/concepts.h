/** @file
 * Conceptos que usan las demos para decidir qué pruebas corren sobre cada contenedor.
 */
#pragma once

#include "common.h"

/** Lista de pares (valor, ref) para llenar un contenedor. */
template <typename Container>
using Values = std::vector<std::pair<typename Container::value_type, Ref>>;

/** Contenedor con `insert(valor, ref)`. */
template <typename C>
concept Insertable = requires(C& c, const typename C::value_type& v, Ref r) { c.insert(v, r); };

/** Contenedor con `top()` y `pop()`. */
template <typename C>
concept HeapLike = requires(C& c) {
    c.top();
    c.pop();
};

/** Árbol de búsqueda: recorridos, altura, `remove` y dibujo. */
template <typename C>
concept TreeLike = requires(C& t, TX key, std::ostream& os) {
    t.InOrder();
    t.PreOrder();
    t.PostOrder();
    t.height();
    t.remove(key);
    t.print_tree(os, visual::Layout::Sideways);
};

/** Árbol que ofrece los seis recorridos (tres órdenes y sus inversos). */
template <typename C>
concept SixOrderTree = requires(const C& t) {
    t.InOrder();
    t.ReverseInOrder();
    t.PreOrder();
    t.ReversePreOrder();
    t.PostOrder();
    t.ReversePostOrder();
};

/** Contenedor que ofrece `ApplyFunction`. */
template <typename C>
concept Mutable = requires(C& c) { c.ApplyFunction([](auto&) {}); };

/** Lista circular. */
template <typename C>
concept Circular = visual::CircularView<C>;

/** Contenedor que se puede copiar y asignar. */
template <typename C>
concept Copyable = std::is_copy_constructible_v<C> && std::is_copy_assignable_v<C>;
