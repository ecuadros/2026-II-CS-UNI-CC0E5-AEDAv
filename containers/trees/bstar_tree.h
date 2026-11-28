/**
 * @file bstar_tree.h
 * @ingroup trees
 * @brief Árbol B* con redistribución entre hermanos y capacidad especial de raíz.
 */

#ifndef __BSTAR_TREE_H__
#define __BSTAR_TREE_H__
#include "btree.h"
#include <stdexcept>

template <typename Traits, template <typename> typename... Mixins>
class BaseBStarTree;

/**
 * @brief Árbol B* estándar con los mismos mixins que `BTree` (incluido `Drawable`); es un `BTree<Traits>`.
 * @tparam Traits `BTreeAscTraits<T>` o `BTreeDescTraits<T>`.
 */
template <typename Traits>
using BStarTree = BaseBStarTree<Traits, Callable, Applicable, Searchable, Reversible, Printable, Readable, Drawable>;

/**
 * @class BaseBStarTree
 * @ingroup trees
 * @brief Especializa la ocupación y el balanceo de `BaseBTree` mediante ganchos virtuales.
 *
 * Con orden `m >= 4`, la capacidad ordinaria es `C = m - 1`, el mínimo
 * ordinario es `L = ⌊2C/3⌋` y la raíz admite `R = 2C + 1` claves.
 * Una página interna no raíz tiene al menos tres hijos.
 *
 * @warning Copie o mueva solo entre árboles del mismo tipo dinámico.
 * Las capacidades de las páginas transferidas deben coincidir con la
 * política del destino. Para convertir, reinserte las claves y sus referencias.
 *
 * @tparam Traits Rasgos del árbol.
 * @tparam Mixins Mixins aplicados al contenedor.
 */
template <typename Traits, template <typename> typename... Mixins>
class BaseBStarTree : public BaseBTree<Traits, Mixins...> {
public:
    using MySelf = BaseBStarTree;
    using Parent = BaseBTree<Traits, Mixins...>;
    using typename Parent::Page;
    using typename Parent::PagePtr;

    static constexpr Size MinOrder = 4;   /**< Orden mínimo admitido para B* (garantiza `L >= 2`). */

    /**
     * @brief Construye un árbol B* vacío.
     * @param order Máximo de hijos por página (`>= MinOrder`).
     * @throws std::invalid_argument Si `order < MinOrder`.
     */
    explicit BaseBStarTree(Size order = Parent::DefaultOrder) : Parent(order) {
        if (order < MinOrder) throw std::invalid_argument("El orden del arbol B* debe ser al menos 4");
    }

protected:
    using Parent::m_order;

    /**
     * @brief Espacio libre de una página no raíz.
     * @param page Página consultada.
     * @return `capacity() - keyCount()`.
     */
    static Size free_cells(const Page& page) { return page.capacity() - page.keyCount(); }

    /**
     * @brief Capacidad: `C = m - 1` en páginas no raíz y `R = 2C + 1 = 2m - 1` en la raíz.
     *
     * `R` se elige para que, al desbordarse (`R + 1` claves) y dividirse en 3
     * con 2 separadores, cada página reciba `2C / 3 >= L` claves.
     * @param isRoot `true` si la página es (o será) la raíz.
     * @return `R` para la raíz, `C` para las demás.
     */
    Size capacity(bool isRoot) const override {
        Size pageCapacity = m_order - 1;
        return isRoot ? 2 * pageCapacity + 1 : pageCapacity;
    }

    /**
     * @brief Mínimo de claves: `L = ⌊2(m - 1)/3⌋` en páginas no raíz; 1 en la raíz.
     * @param page Página consultada.
     * @return Mínimo de claves de `page`.
     */
    Size min_keys(const Page& page) const override {
        if (page.isRoot()) return 1;
        return 2 * (m_order - 1) / 3;
    }

    /**
     * @brief Overflow B*: división de la raíz 1→3, redistribución con un hermano o división 2→3.
     *
     * En las divisiones 1→3 (raíz) y 2→3 (hermanos), hay `2C + 2`
     * claves contando los separadores. Dos quedan en el padre y cada
     * página recibe `⌊2C/3⌋` o `⌈2C/3⌉`.
     * @param page Página desbordada con padre.
     */
    void handle_overflow(PagePtr page) override {
        PagePtr parent = page->parent();
        // La raíz recién creada tiene un único hijo.
        if (parent->keyCount() == 0 && parent->childCount() == 1) {
            this->redistribute_children(parent, 0, 1, 3);
            return;
        }
        Index   pos       = *page->positionInParent();
        PagePtr left      = pos > 0 ? parent->child(pos - 1) : nullptr;
        PagePtr right     = pos + 1 < parent->childCount() ? parent->child(pos + 1) : nullptr;
        Size    freeLeft  = left  ? free_cells(*left)  : Size{};
        Size    freeRight = right ? free_cells(*right) : Size{};

        // Rotar hacia el hermano con más espacio.
        if (freeLeft > 0 && freeLeft > freeRight) {
            while (page->keyCount() > left->keyCount() + 1) this->borrow_from_right(parent, pos - 1);
            return;
        }
        if (freeRight > 0) {
            while (page->keyCount() > right->keyCount() + 1) this->borrow_from_left(parent, pos + 1);
            return;
        }
        // Dividir con el hermano derecho si existe.
        this->redistribute_children(parent, right ? pos : pos - 1, 2, 3);
    }

    /**
     * @brief Underflow B*: préstamo, reparto entre 3 hermanos, absorción por la raíz o fusión 3→2.
     *
     * El reparto 3→3 requiere al menos `3L` claves entre los hermanos.
     * En la fusión 3→2 se reparten `3L` claves y cada página recibe a lo
     * sumo `⌈3L/2⌉ <= C`. Si el padre es una raíz con dos o tres hijos y
     * falla el reparto, los absorbe. El total con separadores es a lo
     * sumo `2L` o `3L + 1`, respectivamente, y cabe en `R`.
     * @param page Página no raíz por debajo del mínimo (puede ser destruida).
     */
    void handle_underflow(PagePtr page) override {
        PagePtr parent = page->parent();
        Index   pos    = *page->positionInParent();
        Size    kids   = parent->childCount();
        Size    need   = min_keys(*page);
        PagePtr left   = pos > 0 ? parent->child(pos - 1) : nullptr;
        PagePtr right  = pos + 1 < kids ? parent->child(pos + 1) : nullptr;
        Size    nLeft  = left  ? left->keyCount()  : Size{};
        Size    nRight = right ? right->keyCount() : Size{};

        // Pedir prestado al hermano con más claves.
        if (nLeft > need && nLeft > nRight) {
            while (page->keyCount() + 1 < left->keyCount()) this->borrow_from_left(parent, pos);
            return;
        }
        if (nRight > need) {
            while (page->keyCount() + 1 < right->keyCount()) this->borrow_from_right(parent, pos);
            return;
        }

        // Ajustar la ventana de tres hermanos en los extremos.
        Index first{};
        if (kids >= 3) {
            first = pos == 0 ? Index{} : (pos + 1 == kids ? pos - 2 : pos - 1);
            Size total{};
            for (Index i = first; i < first + 3; ++i) total += parent->child(i)->keyCount();
            if (total >= 3 * need) {
                this->redistribute_children(parent, first, 3, 3);
                return;
            }
        }

        // La absorción reduce la altura del árbol.
        if (parent->isRoot() && kids <= 3) {
            this->absorb_children(parent);
            return;
        }

        if (kids >= 3) {
            this->redistribute_children(parent, first, 3, 2);
            return;
        }
        throw std::logic_error("BStarTree: pagina interna no raiz con menos de 3 hijos");
    }
};

#endif // __BSTAR_TREE_H__
