/**
 * @file bstar_tree.h
 * @ingroup trees
 * @brief Árbol B* (`BaseBStarTree` / `BStarTree`) como especialización polimórfica de `BaseBTree`.
 *
 * `BaseBStarTree` reutiliza íntegro el algoritmo común de `BaseBTree`
 * (descenso, inserción en hoja, propagación por `parent()`, crecimiento y
 * contracción de la raíz) y solo sobrescribe sus ganchos virtuales
 * (Template Method): `capacity`, `min_keys`, `handle_overflow` y
 * `handle_underflow`. Un `BStarTree<Traits>` ES un `BTree<Traits>`: puede
 * usarse a través de `BTree<Traits>&` y las operaciones despachan a la
 * política B*.
 *
 * Notación: `m` = orden (máximo de hijos), `C = m - 1` = capacidad de una
 * página no raíz, `L` = mínimo de claves de una página no raíz, `R` =
 * capacidad de la raíz.
 *
 * | Aspecto                        | Árbol B clásico (`BTree`)     | Árbol B* (`BStarTree`)                              |
 * |--------------------------------|-------------------------------|-----------------------------------------------------|
 * | Ocupación mínima (no raíz)     | ~1/2: `L = ⌊(m-1)/2⌋`         | ~2/3: `L = ⌊2(m-1)/3⌋ = ⌊2C/3⌋`                     |
 * | Capacidad de la raíz           | `C = m - 1`                   | `R = 2C + 1 = 2m - 1` (más capacidad)               |
 * | Overflow                       | división inmediata            | primero redistribuye con un hermano con espacio     |
 * | División                       | 1→2 (sube la mediana)         | 2→3 (página + hermano lleno → 3 páginas, 2 separadores) |
 * | División de la raíz            | 1→2                           | 1→3 (`SplitRoot`)                                   |
 * | Underflow                      | préstamo o fusión 2→1         | préstamo, reparto entre 3 hermanos o fusión 3→2     |
 * | Contracción de la raíz         | raíz vacía → su único hijo    | la raíz absorbe a sus 2-3 hijos (`MergeRoot`)       |
 * | Orden mínimo admitido          | 3                             | 4                                                   |
 *
 * **Por qué estas fórmulas respetan mínimo y capacidad:**
 * - División 2→3: página desbordada (`C + 1`) + hermano lleno (`C`) + separador
 *   = `2C + 2` claves; 2 suben al padre y quedan `2C` para 3 páginas:
 *   cada una recibe `⌊2C/3⌋ = L` o `⌈2C/3⌉ ≤ C`.
 * - División de la raíz: `R + 1 = 2C + 2` claves; 2 quedan en la raíz nueva
 *   y `2C` se reparten en 3 páginas, exactamente como en la división 2→3.
 * - Fusión 3→2: tres hermanos con `L - 1`, `L`, `L` claves + 2 separadores
 *   = `3L + 1`; 1 queda como separador y `3L` se reparten en 2 páginas:
 *   `⌈3L/2⌉ ≤ C` porque `L ≤ 2C/3`.
 * - Absorción de la raíz: con 2 hijos `2L ≤ R`; con 3 hijos `3L + 1 ≤ 2C + 1 = R`.
 * - Orden mínimo 4: garantiza `L ≥ 2`, es decir, toda página interna no raíz
 *   tiene al menos 3 hijos, así que siempre existe la ventana de 3 hermanos
 *   y nunca se necesita una fusión 2→1 (que con `2L > C` no cabría).
 *   Con `m = 3` (`L = 1`) una página interna no raíz podría tener solo 2 hijos.
 *
 * **Diferencias con el código del profesor (`CBTreePage`):**
 * - El orden es el máximo de hijos (`m`) en vez del máximo de claves de las
 *   hijas (`order`); la relación entre raíz e hijas se conserva
 *   (`R = 2C + 1`, como `2 * order + 1`).
 * - El mínimo se calcula en aritmética entera (`2C / 3`), sin la conversión
 *   implícita `double`→entero de `2 * m_MaxKeys / 3.0`; el valor es el mismo.
 * - Las rotaciones (`RedistributeL2R` / `R2L`) se expresan con
 *   `borrow_from_left` / `borrow_from_right` de la base, repetidas hasta
 *   equilibrar (mismo criterio que el `while` del legacy).
 * - `Redistribute2`, `SplitChild` + `SplitPageInto3`, `Merge` y `SplitRoot`
 *   se unifican en una sola primitiva, `redistribute_children(parent, first, k, k2)`,
 *   que reparte de forma equitativa; no se exige el conteo exacto que el
 *   legacy verificaba con `assert`.
 * - `MergeRoot` del legacy solo se aplica con 2 claves en la raíz; aquí
 *   también con 1 clave (2 hijos), caso en que el `Redistribute2` del legacy
 *   no tiene ventana de 3 hermanos (su `assert` fallaría).
 * - Al borrar en una página interna la base usa el predecesor (el legacy usa
 *   el sucesor, `GetFirstObjectInfo` de la rama derecha); ambos son válidos.
 * - Claves únicas sin `UseCounter`; páginas con dueño único (`unique_ptr`)
 *   y enlace al padre mantenido por las primitivas, sin `delete this`.
 */

#ifndef __BSTAR_TREE_H__
#define __BSTAR_TREE_H__
#include "btree.h"
#include <stdexcept>

template <typename Traits, template <typename> typename... Mixins>
class BaseBStarTree;

/** @brief Árbol B* estándar con los mismos mixins que `BTree` (incluido `Drawable`); es un `BTree<Traits>`. */
template <typename Traits>
using BStarTree = BaseBStarTree<Traits, Callable, Applicable, Searchable, Reversible, Printable, Readable, Drawable>;

/**
 * @class BaseBStarTree
 * @brief Árbol B* genérico y thread-safe: ocupación mínima 2/3, redistribución antes de dividir, división 2→3 y fusión 3→2.
 *
 * Solo sobrescribe los ganchos de `BaseBTree`; insertar, borrar, buscar,
 * copiar, mover e iterar se heredan sin cambios.
 *
 * @warning Object slicing: no copie ni asigne un `BaseBStarTree` hacia o
 * desde un `BaseBTree` clásico a través de `BaseBTree&`. La copia es la de
 * la base y conserva las capacidades de las páginas del tipo origen (raíz
 * `2m-1` en B*, `m-1` en el clásico), que no coinciden con los ganchos del
 * destino. Copie solo entre árboles del mismo tipo; para convertir, reinserte
 * las claves (p. ej. escribiendo con `<<` y leyendo con `>>`).
 *
 * @tparam Traits Rasgos del árbol (`BTreeAscTraits` o `BTreeDescTraits`).
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

    /** @brief Espacio libre de una página no raíz. */
    static Size free_cells(const Page& page) { return page.capacity() - page.keyCount(); }

    /**
     * @brief Capacidad: `C = m - 1` en páginas no raíz y `R = 2C + 1 = 2m - 1` en la raíz.
     *
     * `R` se elige para que, al desbordarse (`R + 1` claves) y dividirse en 3
     * con 2 separadores, cada página reciba `2C / 3 >= L` claves.
     * @param isRoot `true` si la página es (o será) la raíz.
     */
    Size capacity(bool isRoot) const override {
        Size pageCapacity = m_order - 1;
        return isRoot ? 2 * pageCapacity + 1 : pageCapacity;
    }

    /**
     * @brief Mínimo de claves: `L = ⌊2(m - 1)/3⌋` en páginas no raíz; 1 en la raíz.
     * @param page Página consultada.
     */
    Size min_keys(const Page& page) const override {
        if (page.isRoot()) return 1;
        return 2 * (m_order - 1) / 3;
    }

    /**
     * @brief Overflow B*: división de la raíz 1→3, redistribución con un hermano o división 2→3.
     * @param page Página con `isOverflow() == true` (no raíz).
     */
    void handle_overflow(PagePtr page) override {
        PagePtr parent = page->parent();
        // (a) Raíz recién creada por grow_root: 0 claves y un único hijo → SplitRoot 1→3.
        if (parent->keyCount() == 0 && parent->childCount() == 1) {
            this->redistribute_children(parent, 0, 1, 3);
            return;
        }
        Index   pos       = *page->positionInParent();
        PagePtr left      = pos > 0 ? parent->child(pos - 1) : nullptr;
        PagePtr right     = pos + 1 < parent->childCount() ? parent->child(pos + 1) : nullptr;
        Size    freeLeft  = left  ? free_cells(*left)  : Size{};
        Size    freeRight = right ? free_cells(*right) : Size{};

        // (b) Redistribute1: rotar hacia el hermano con más espacio hasta equilibrar.
        if (freeLeft > 0 && freeLeft > freeRight) {
            while (page->keyCount() > left->keyCount() + 1) this->borrow_from_right(parent, pos - 1);
            return;
        }
        if (freeRight > 0) {
            while (page->keyCount() > right->keyCount() + 1) this->borrow_from_left(parent, pos + 1);
            return;
        }
        // (c) Ambos hermanos llenos: división 2→3 con el derecho (o el izquierdo si no hay derecho).
        this->redistribute_children(parent, right ? pos : pos - 1, 2, 3);
    }

    /**
     * @brief Underflow B*: préstamo, reparto entre 3 hermanos, absorción por la raíz o fusión 3→2.
     * @param page Página no raíz con menos de `min_keys()` claves (puede ser destruida).
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

        // 1. Préstamo del hermano adyacente con más claves, equilibrando ambos.
        if (nLeft > need && nLeft > nRight) {
            while (page->keyCount() + 1 < left->keyCount()) this->borrow_from_left(parent, pos);
            return;
        }
        if (nRight > need) {
            while (page->keyCount() + 1 < right->keyCount()) this->borrow_from_right(parent, pos);
            return;
        }

        // 2. Redistribute2: ventana de 3 hermanos consecutivos (desplazada en los extremos).
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

        // 3. MergeRoot: la raíz con 2 o 3 hijos los absorbe y la altura baja en 1.
        if (parent->isRoot() && kids <= 3) {
            this->absorb_children(parent);
            return;
        }

        // 4. Merge 3→2 (el padre pierde un separador y puede quedar en underflow).
        if (kids >= 3) {
            this->redistribute_children(parent, first, 3, 2);
            return;
        }
        throw std::logic_error("BStarTree: pagina interna no raiz con menos de 3 hijos");
    }
};

#endif // __BSTAR_TREE_H__
