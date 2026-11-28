/**
 * @file btree_metrics.h
 * @ingroup trees
 * @brief Altura, número de páginas y ocupación de árboles B y B*.
 * @note Los porcentajes se calculan en décimas con aritmética entera.
 * @warning Proteja la lectura completa si otros hilos modifican el árbol.
 */

#ifndef __BTREE_METRICS_H__
#define __BTREE_METRICS_H__

#include <algorithm>
#include <string>
#include "../types.h"

/** @brief Métricas estructurales de un árbol B. */
struct BTreeShape {
    Size height{};          /**< Altura en páginas. */
    Size pages{};           /**< Páginas totales. */
    Size nonRootPages{};    /**< Páginas que no son raíz. */
    Size nonRootKeys{};     /**< Claves en páginas no raíz. */
    Size belowMin{};        /**< Páginas no raíz con menos claves que el mínimo de referencia. */
};

/**
 * @brief Recorre recursivamente las páginas acumulando métricas.
 * @tparam Page Tipo de página.
 * @tparam Reference Tipo del árbol de referencia.
 * @param page Página actual.
 * @param depth Profundidad (raíz = 1).
 * @param reference Árbol cuyo `minKeysFor()` define el mínimo de referencia.
 * @param shape Acumulador.
 */
template <typename Page, typename Reference>
void btree_measure(const Page* page, Size depth, const Reference& reference, BTreeShape& shape) {
    ++shape.pages;
    shape.height = std::max(shape.height, depth);
    if (!page->isRoot()) {
        ++shape.nonRootPages;
        shape.nonRootKeys += page->keyCount();
        if (page->keyCount() < reference.minKeysFor(*page)) ++shape.belowMin;
    }
    for (Index i{}; i < page->childCount(); ++i) btree_measure(page->child(i), depth + 1, reference, shape);
}

/**
 * @brief Métricas de `tree` usando el mínimo de `reference` (p. ej. el 2/3 de un B* sobre un árbol clásico).
 * @tparam Tree Tipo del árbol medido.
 * @tparam Reference Tipo del árbol de referencia.
 * @param tree Árbol a medir.
 * @param reference Árbol que define el mínimo de claves (puede ser el mismo).
 * @return Métricas acumuladas.
 */
template <typename Tree, typename Reference>
BTreeShape btree_shape(const Tree& tree, const Reference& reference) {
    BTreeShape shape;
    if (tree.getRoot()) btree_measure(tree.getRoot(), 1, reference, shape);
    return shape;
}

/**
 * @brief Métricas de `tree` usando su propio mínimo.
 * @tparam Tree Tipo del árbol.
 * @param tree Árbol a medir.
 * @return Métricas acumuladas.
 */
template <typename Tree>
BTreeShape btree_shape(const Tree& tree) { return btree_shape(tree, tree); }

/**
 * @brief Ocupación de páginas no raíz en décimas de porcentaje.
 * @param shape Métricas del árbol.
 * @param pageCapacity Capacidad de una página no raíz (`capacityFor(false)`).
 * @return `1000 * claves / (páginas * capacidad)`; 0 si no hay páginas no raíz.
 */
inline Size btree_occupancy_tenths(const BTreeShape& shape, Size pageCapacity) {
    Size slots = shape.nonRootPages * pageCapacity;
    return slots == 0 ? Size{} : Size{1000} * shape.nonRootKeys / slots;
}

/** @brief Formatea décimas de porcentaje como `NN.N%`. @param tenths Décimas de porcentaje. @return Texto `NN.N%`. */
inline std::string btree_percent(Size tenths) {
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10) + "%";
}

#endif // __BTREE_METRICS_H__
