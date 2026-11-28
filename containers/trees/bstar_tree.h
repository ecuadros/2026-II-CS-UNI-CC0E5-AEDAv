/**
 * @file bstar_tree.h
 * @ingroup trees
 * @brief Árbol B*: redistribución entre hermanos y raíz con mayor capacidad.
 */

#ifndef __BSTAR_TREE_H__
#define __BSTAR_TREE_H__
#include "btree.h"
#include <stdexcept>

template <typename Traits, template <typename> typename... Mixins>
class BaseBStarTree;

/// Árbol B* con los mismos mixins que `BTree`.
template <typename Traits>
using BStarTree = BaseBStarTree<Traits, Callable, Applicable, Searchable, Reversible, Printable, Readable, Drawable>;

/**
 * @class BaseBStarTree
 * @ingroup trees
 * @brief Árbol B* sobre `BaseBTree` (redefine capacidad, mínimo y balanceo).
 *
 * Con orden `m >= 4`: capacidad `C = m - 1`, mínimo `L = ⌊2C/3⌋`,
 * raíz `R = 2C + 1`.
 *
 * @warning Copie o mueva solo entre árboles del mismo tipo.
 */
template <typename Traits, template <typename> typename... Mixins>
class BaseBStarTree : public BaseBTree<Traits, Mixins...> {
public:
    using MySelf = BaseBStarTree;
    using Parent = BaseBTree<Traits, Mixins...>;
    using typename Parent::Page;
    using typename Parent::PagePtr;

    static constexpr Size MinOrder = 4;   ///< Garantiza `L >= 2`.

    /// @throws std::invalid_argument Si `order < MinOrder`.
    explicit BaseBStarTree(Size order = Parent::DefaultOrder) : Parent(order) {
        if (order < MinOrder) throw std::invalid_argument("El orden del arbol B* debe ser al menos 4");
    }

protected:
    using Parent::m_order;

    /// Espacio libre de una página.
    static Size free_cells(const Page& page) { return page.capacity() - page.keyCount(); }

    /// `R = 2C + 1` en la raíz (al dividirse en 3, cada página recibe `>= L`); `C` en las demás.
    Size capacity(bool isRoot) const override {
        Size pageCapacity = m_order - 1;
        return isRoot ? 2 * pageCapacity + 1 : pageCapacity;
    }

    /// `L` en páginas no raíz; 1 en la raíz.
    Size min_keys(const Page& page) const override {
        if (page.isRoot()) return 1;
        return 2 * (m_order - 1) / 3;
    }

    /// Overflow: división de raíz 1→3, rotación a un hermano o división 2→3.
    void handle_overflow(PagePtr page) override {
        PagePtr parent = page->parent();
        // Raíz recién creada con un único hijo.
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
        // Dividir 2→3 (preferir hermano derecho).
        this->redistribute_children(parent, right ? pos : pos - 1, 2, 3);
    }

    /// Underflow: préstamo, reparto 3→3, absorción por la raíz o fusión 3→2.
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

        // Reparto 3→3 si hay al menos 3L claves.
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

        // La raíz absorbe a sus hijos (reduce la altura).
        if (parent->isRoot() && kids <= 3) {
            this->absorb_children(parent);
            return;
        }

        // Fusión 3→2.
        if (kids >= 3) {
            this->redistribute_children(parent, first, 3, 2);
            return;
        }
        throw std::logic_error("BStarTree: pagina interna no raiz con menos de 3 hijos");
    }
};

#endif // __BSTAR_TREE_H__
