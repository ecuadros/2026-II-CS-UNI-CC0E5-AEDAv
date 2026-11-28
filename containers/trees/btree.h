/**
 * @file btree.h
 * @ingroup trees
 * @brief Árbol B clásico (`BaseBTree` / `BTree`), su página (`BTreePage`), iterador sin pila (`BTreeIterator`) y rasgos de orden.
 *
 * La página es la unidad estructural del árbol B: almacena claves ordenadas
 * como `GeneralNode<T>` (valor + `Ref`) dentro de un `BaseVector`, posee a sus
 * hijos mediante `std::unique_ptr` y mantiene un puntero no propietario a su
 * padre. Todas sus primitivas de modificación conservan el enlace al padre.
 *
 * La página NO decide políticas de ocupación mínima, redistribución ni
 * división: esas decisiones corresponden al árbol (`BaseBTree`), que las
 * expone como ganchos virtuales (Template Method) para que variantes como el
 * árbol B* solo sobrescriban la política local.
 */

#ifndef __BTREE_H__
#define __BTREE_H__
#include "../core/general_node.h"
#include "../core/general_container.h"
#include "../core/general_iterator.h"
#include "../core/mixins.h"
#include "../core/direction.h"
#include "../vector.h"
#include "../../types.h"
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

/**
 * @struct BTreeKeyTraits
 * @brief Rasgos del `BaseVector` que guarda las claves de una página.
 *
 * Combina el orden (`DefaultTraits`) con los nodos e iteradores de vector,
 * sin redefinir `Compare`.
 *
 * @tparam T Tipo de la clave.
 * @tparam Compare Comparador estricto de claves.
 */
template <typename T, typename Compare>
struct BTreeKeyTraits : public DefaultTraits<T, Compare>, public VectorTraits<T> {};

/**
 * @class BTreePage
 * @ingroup trees
 * @brief Página con claves ordenadas, hijos propios y enlace no propietario al padre.
 *
 * Cada hijo pertenece a un `std::unique_ptr`. Al insertar o extraer un hijo se
 * actualiza su enlace al padre. Una página interna estable tiene una clave
 * menos que hijos. Las primitivas permiten estados intermedios de balanceo.
 *
 * @tparam T Tipo de la clave.
 * @tparam Compare Comparador estricto de claves.
 */
template <typename T, typename Compare>
class BTreePage {
public:
    using value_type = T;
    using MySelf     = BTreePage<T, Compare>;
    using KeyTraits  = BTreeKeyTraits<T, Compare>;
    using Keys       = BaseVector<KeyTraits>;
    using Node       = typename Keys::Node;      /**< `GeneralNode<T>`: valor + `Ref`. */
    using PagePtr    = MySelf*;                  /**< Puntero no propietario a página. */
    using PageOwner  = std::unique_ptr<MySelf>;  /**< Puntero propietario a página. */
    using Children   = std::vector<PageOwner>;

private:
    Keys     m_keys;               /**< Claves ordenadas de la página. */
    Children m_children;           /**< Hijos propios (vacío si la página es hoja). */
    PagePtr  m_parent = nullptr;   /**< Padre no propietario (`nullptr` en la raíz). */
    Size     m_capacity;           /**< Máximo de claves permitido en estado estable. */

    /**
     * @brief Verifica que una posición esté dentro del rango válido.
     * @param pos Posición a verificar.
     * @param limit Límite exclusivo.
     * @param what Mensaje de la excepción.
     * @throws std::out_of_range Si `pos >= limit`.
     */
    static void checkRange(Index pos, Size limit, const char* what) {
        if (pos >= limit) throw std::out_of_range(what);
    }

public:
    /**
     * @brief Construye una página vacía (hoja) con la capacidad indicada.
     * @param capacity Número máximo de claves en estado estable.
     * @throws std::invalid_argument Si `capacity` es 0.
     */
    explicit BTreePage(Size capacity) : m_capacity(capacity) {
        if (capacity == 0) throw std::invalid_argument("La capacidad de la pagina debe ser positiva");
        m_keys.reserve(capacity + 1);
    }

    /** @brief Destructor virtual; los hijos se liberan por `unique_ptr`. */
    virtual ~BTreePage() = default;

    BTreePage(const BTreePage&)            = delete;
    BTreePage& operator=(const BTreePage&) = delete;
    BTreePage(BTreePage&&)                 = delete;
    BTreePage& operator=(BTreePage&&)      = delete;

    // ---------------------------------------------------------------- consultas

    /** @brief Capacidad máxima de claves en estado estable. */
    Size capacity()   const { return m_capacity; }
    /** @brief Cantidad de claves almacenadas. */
    Size keyCount()   const { return m_keys.size(); }
    /** @brief Cantidad de hijos. */
    Size childCount() const { return m_children.size(); }
    /** @brief Indica si la página no tiene hijos. @return `true` si es hoja. */
    bool isLeaf()     const { return m_children.empty(); }
    /** @brief Indica si la página no tiene padre. @return `true` si es raíz. */
    bool isRoot()     const { return m_parent == nullptr; }
    /** @brief Indica si la página alcanzó su capacidad. @return `keyCount() >= capacity()`. */
    bool isFull()     const { return keyCount() >= m_capacity; }
    /** @brief Indica si la página excede su capacidad (estado transitorio de inserción). @return `keyCount() > capacity()`. */
    bool isOverflow() const { return keyCount() > m_capacity; }

    /** @brief Padre no propietario (`nullptr` en la raíz). */
    PagePtr parent() const { return m_parent; }

    /**
     * @brief Cambia la capacidad máxima (cuando la página pasa de raíz a hija o viceversa).
     *
     * No reorganiza claves: si queda en overflow, corregirlo es responsabilidad del árbol.
     * @param capacity Nueva capacidad.
     * @throws std::invalid_argument Si `capacity` es 0.
     */
    void setCapacity(Size capacity) {
        if (capacity == 0) throw std::invalid_argument("La capacidad de la pagina debe ser positiva");
        m_capacity = capacity;
    }

    /**
     * @brief Nodo (valor + `Ref`) en la posición `pos`.
     * @param pos Posición de la clave.
     * @return Referencia constante al nodo.
     * @throws std::out_of_range Si `pos >= keyCount()`.
     */
    const Node& key(Index pos) const {
        checkRange(pos, keyCount(), "Posicion de clave fuera de rango");
        return m_keys[pos];
    }

    /**
     * @brief Nodo mutable en la posición `pos` (para actualizar `Ref` o reemplazar separadores).
     * @param pos Posición de la clave.
     * @return Referencia mutable al nodo.
     * @throws std::out_of_range Si `pos >= keyCount()`.
     */
    Node& key(Index pos) {
        checkRange(pos, keyCount(), "Posicion de clave fuera de rango");
        return m_keys[pos];
    }

    /**
     * @brief Hijo no propietario en la posición `pos`.
     * @param pos Posición del hijo.
     * @return Puntero a la página hija.
     * @throws std::out_of_range Si `pos >= childCount()`.
     */
    PagePtr child(Index pos) const {
        checkRange(pos, childCount(), "Posicion de hijo fuera de rango");
        return m_children[pos].get();
    }

    // ---------------------------------------------------------------- búsqueda

    /**
     * @brief Búsqueda binaria: primera posición cuya clave no precede a `value` según `Compare`.
     *
     * En una página interna, la posición retornada es también el índice del
     * hijo por el que debe descenderse cuando la clave no está en la página.
     *
     * @param value Valor buscado.
     * @return Posición en `[0, keyCount()]`.
     */
    Index lowerBound(const value_type& value) const {
        Index first{};
        Index last = keyCount();
        while (first < last) {
            Index mid = first + (last - first) / 2;
            if (Compare{}(m_keys[mid].getValue(), value)) first = mid + 1;
            else                                          last  = mid;
        }
        return first;
    }

    /**
     * @brief Indica si la clave en `pos` es equivalente a `value` (ni precede ni sucede).
     * @param pos Posición a comparar (típicamente el resultado de `lowerBound()`).
     * @param value Valor de referencia.
     * @return `true` si `pos < keyCount()` y la clave es equivalente.
     */
    bool matchesAt(Index pos, const value_type& value) const {
        return pos < keyCount() && !Compare{}(value, m_keys[pos].getValue())
                                && !Compare{}(m_keys[pos].getValue(), value);
    }

    /**
     * @brief Busca una clave equivalente a `value` solo en esta página.
     * @param value Valor buscado.
     * @return Posición de la clave o `std::nullopt` si no está.
     */
    std::optional<Index> find(const value_type& value) const {
        Index pos = lowerBound(value);
        if (matchesAt(pos, value)) return pos;
        return std::nullopt;
    }

    // ---------------------------------------------------------------- claves

    /**
     * @brief Inserta un nodo en la posición `pos`, desplazando a la derecha los siguientes.
     * @param pos Posición destino en `[0, keyCount()]`.
     * @param node Nodo (valor + `Ref`) a insertar.
     * @throws std::out_of_range Si `pos > keyCount()`.
     */
    void insertKey(Index pos, const Node& node) {
        checkRange(pos, keyCount() + 1, "Posicion de insercion de clave fuera de rango");
        m_keys.push_back(node.getValue(), node.getRef());
        for (Index i = keyCount() - 1; i > pos; --i)
            m_keys[i] = m_keys[i - 1];
        m_keys[pos] = node;
    }

    /**
     * @brief Inserta `(value, ref)` en la posición `pos`.
     * @param pos Posición destino en `[0, keyCount()]`.
     * @param value Valor de la clave.
     * @param ref Referencia asociada.
     * @throws std::out_of_range Si `pos > keyCount()`.
     * @see insertKey(Index, const Node&)
     */
    void insertKey(Index pos, const value_type& value, Ref ref) { insertKey(pos, Node(value, ref)); }

    /**
     * @brief Inserta `(value, ref)` en su posición ordenada.
     * @param value Valor a insertar.
     * @param ref Referencia asociada.
     * @return Posición donde quedó la clave.
     */
    Index insertSorted(const value_type& value, Ref ref) {
        Index pos = lowerBound(value);
        insertKey(pos, value, ref);
        return pos;
    }

    /**
     * @brief Elimina la clave en `pos`, desplazando a la izquierda las siguientes.
     * @param pos Posición de la clave.
     * @return El nodo eliminado.
     * @throws std::out_of_range Si `pos >= keyCount()`.
     */
    Node eraseKey(Index pos) {
        checkRange(pos, keyCount(), "Posicion de eliminacion de clave fuera de rango");
        Node removed = m_keys[pos];
        for (Index i = pos + 1; i < keyCount(); ++i)
            m_keys[i - 1] = m_keys[i];
        m_keys.pop_back();
        return removed;
    }

    // ---------------------------------------------------------------- hijos

    /**
     * @brief Inserta un hijo en la posición `pos` y lo enlaza a esta página como padre.
     * @param pos Posición destino en `[0, childCount()]`.
     * @param page Página hija (se transfiere su propiedad).
     * @return Puntero no propietario al hijo insertado.
     * @throws std::out_of_range Si `pos > childCount()`.
     * @throws std::invalid_argument Si `page` es nulo.
     */
    PagePtr insertChild(Index pos, PageOwner page) {
        checkRange(pos, childCount() + 1, "Posicion de insercion de hijo fuera de rango");
        if (!page) throw std::invalid_argument("No se puede insertar un hijo nulo");
        page->m_parent = this;
        PagePtr raw = page.get();
        m_children.insert(m_children.begin() + pos, std::move(page));
        return raw;
    }

    /**
     * @brief Extrae el hijo en `pos`, transfiriendo su propiedad y desenlazando su padre.
     * @param pos Posición del hijo.
     * @return La página extraída con `parent() == nullptr`.
     * @throws std::out_of_range Si `pos >= childCount()`.
     */
    PageOwner removeChild(Index pos) {
        checkRange(pos, childCount(), "Posicion de eliminacion de hijo fuera de rango");
        PageOwner page = std::move(m_children[pos]);
        m_children.erase(m_children.begin() + pos);
        page->m_parent = nullptr;
        return page;
    }

    /**
     * @brief Posición de `page` entre los hijos de esta página.
     * @param page Página buscada.
     * @return Su posición o `std::nullopt` si no es hija directa.
     */
    std::optional<Index> indexOfChild(const MySelf* page) const {
        for (Index i{}; i < childCount(); ++i)
            if (m_children[i].get() == page) return i;
        return std::nullopt;
    }

    /**
     * @brief Posición de esta página entre los hijos de su padre.
     * @return La posición o `std::nullopt` si la página es raíz.
     */
    std::optional<Index> positionInParent() const {
        if (!m_parent) return std::nullopt;
        return m_parent->indexOfChild(this);
    }

    // ---------------------------------------------------------------- copia y validación

    /**
     * @brief Copia profunda del subárbol con raíz en esta página.
     *
     * La copia es independiente (no comparte páginas) y su raíz no tiene padre.
     * @return Página raíz de la copia.
     */
    PageOwner clone() const {
        auto copy = std::make_unique<MySelf>(m_capacity);
        copy->m_keys = m_keys;
        for (const PageOwner& c : m_children)
            copy->insertChild(copy->childCount(), c->clone());
        return copy;
    }

    /**
     * @brief Verifica recursivamente la forma del subárbol.
     *
     * Comprueba: claves en orden no decreciente según `Compare`, hojas sin
     * hijos o internas con `keyCount() + 1` hijos, y enlace correcto al padre
     * en cada hijo. No verifica ocupación mínima (es política del árbol).
     * @return `true` si todas las condiciones se cumplen.
     */
    bool isWellFormed() const {
        for (Index i = 1; i < keyCount(); ++i)
            if (Compare{}(m_keys[i].getValue(), m_keys[i - 1].getValue())) return false;
        if (isLeaf()) return true;
        if (childCount() != keyCount() + 1) return false;
        for (const PageOwner& c : m_children)
            if (c->m_parent != this || !c->isWellFormed()) return false;
        return true;
    }
};


/**
 * @class BTreeIterator
 * @ingroup trees
 * @brief Iterador in-order con espacio auxiliar O(1), basado en `GeneralIterator` (CRTP).
 *
 * Guarda la página y la posición actuales. Avanza dentro de la hoja, baja
 * al subárbol siguiente o sube por `parent()` y `positionInParent()` hasta
 * encontrar un separador pendiente. Una página nula representa el fin.
 *
 * @tparam T Tipo de la clave.
 * @tparam Compare Comparador del árbol.
 * @tparam Dir `Direction::Asc` sigue a `Compare` y `Direction::Desc` invierte el recorrido.
 */
template <typename T, typename Compare, Direction Dir>
class BTreeIterator : public GeneralIterator<BTreeIterator<T, Compare, Dir>, GeneralNode<T>> {
public:
    using Node    = GeneralNode<T>;
    using Page    = BTreePage<T, Compare>;
    using PagePtr = Page*;
    using MySelf  = BTreeIterator<T, Compare, Dir>;
    using Parent  = GeneralIterator<MySelf, Node>;
    using Parent::m_ptr;

private:
    static constexpr bool Forward = (Dir == Direction::Asc);

    PagePtr m_page = nullptr;   /**< Página actual (`nullptr` en `end()`). */
    Index   m_pos{};            /**< Posición de la clave actual en `m_page`. */

    /** @brief Actualiza `m_ptr` (usado por `operator*`) a partir de `(m_page, m_pos)`. */
    void sync() {
        if (!m_page) m_pos = 0;
        m_ptr = m_page ? &m_page->key(m_pos) : nullptr;
    }

    /**
     * @brief Se posiciona en la primera clave (según `Dir`) del subárbol con raíz `page`.
     * @param page Raíz del subárbol (no nula y con claves).
     */
    void descend(PagePtr page) {
        if constexpr (Forward) {
            while (!page->isLeaf()) page = page->child(0);
            m_pos = 0;
        } else {
            while (!page->isLeaf()) page = page->child(page->childCount() - 1);
            m_pos = page->keyCount() - 1;
        }
        m_page = page;
    }

    /** @brief Sube por los padres hasta el primer ancestro con clave pendiente; si no hay, queda en `end()`. */
    void climb() {
        PagePtr page = m_page;
        while (std::optional<Index> slot = page->positionInParent()) {
            page = page->parent();
            if constexpr (Forward) {
                if (*slot < page->keyCount()) { m_page = page; m_pos = *slot; return; }
            } else {
                if (*slot > 0) { m_page = page; m_pos = *slot - 1; return; }
            }
        }
        m_page = nullptr;
    }

public:
    /**
     * @brief Construye el iterador en la primera clave del árbol con raíz `root`.
     * @param root Raíz del árbol; `nullptr` (o raíz sin claves) produce `end()`.
     */
    explicit BTreeIterator(PagePtr root = nullptr) : Parent(nullptr) {
        if (root && root->keyCount() > 0) descend(root);
        sync();
    }

    /** @brief Avanza a la siguiente clave en la dirección `Dir`. @return `*this`. */
    MySelf& operator++() {
        if (!m_page) return *this;
        if constexpr (Forward) {
            if (!m_page->isLeaf())                    descend(m_page->child(m_pos + 1));
            else if (m_pos + 1 < m_page->keyCount())  ++m_pos;
            else                                      climb();
        } else {
            if (!m_page->isLeaf())                    descend(m_page->child(m_pos));
            else if (m_pos > 0)                       --m_pos;
            else                                      climb();
        }
        sync();
        return *this;
    }

    /** @brief Página actual; útil para inspección y pruebas. @return Página (`nullptr` en `end()`). */
    PagePtr page()     const { return m_page; }
    /** @brief Posición de la clave actual. @return Índice dentro de `page()`. */
    Index   position() const { return m_pos; }

    /**
     * @brief Igualdad coherente con `end()`: misma página y misma posición.
     * @param a Primer iterador.
     * @param b Segundo iterador.
     * @return `true` si apuntan a la misma clave (o ambos son `end()`).
     */
    friend bool operator==(const MySelf& a, const MySelf& b) {
        return a.m_page == b.m_page && a.m_pos == b.m_pos;
    }
    /**
     * @brief Desigualdad.
     * @param a Primer iterador.
     * @param b Segundo iterador.
     * @return `!(a == b)`.
     */
    friend bool operator!=(const MySelf& a, const MySelf& b) { return !(a == b); }
};

/**
 * @struct BTreeTraits
 * @ingroup trees
 * @brief Define el nodo de clave, la página y los iteradores del árbol B.
 * @tparam T Tipo de la clave.
 * @tparam Cmp Comparador aportado por los rasgos de orden.
 */
template <typename T, typename Cmp>
struct BTreeTraits {
    using Node             = GeneralNode<T>;                       /**< Nodo de clave (valor + `Ref`). */
    using Page             = BTreePage<T, Cmp>;                    /**< Página del árbol. */
    using ForwardIterator  = BTreeIterator<T, Cmp, Direction::Asc>;
    using BackwardIterator = BTreeIterator<T, Cmp, Direction::Desc>;
};

/** @brief Rasgos de árbol B con orden ascendente (`std::less<T>`). @tparam T Tipo de la clave. */
template <typename T>
struct BTreeAscTraits : public AscendingTraits<T>,
                        public BTreeTraits<T, typename AscendingTraits<T>::Compare> {};

/** @brief Rasgos de árbol B con orden descendente (`std::greater<T>`). @tparam T Tipo de la clave. */
template <typename T>
struct BTreeDescTraits : public DescendingTraits<T>,
                         public BTreeTraits<T, typename DescendingTraits<T>::Compare> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseBTree;

/**
 * @brief Árbol B estándar con mixins de consulta, mutación, reversa, serialización y dibujo (`BTreeLayout`).
 * @tparam Traits `BTreeAscTraits<T>` o `BTreeDescTraits<T>`.
 */
template <typename Traits>
using BTree = BaseBTree<Traits, Callable, Applicable, Searchable, Reversible, Printable, Readable, Drawable>;

/**
 * @class BaseBTree
 * @ingroup trees
 * @brief Árbol B de claves únicas integrado con `GeneralContainer`.
 *
 * El orden `m` es el máximo ordinario de hijos. En el árbol clásico el
 * máximo es `m - 1` claves por página y el mínimo ordinario es
 * `⌊(m - 1)/2⌋`. Admite órdenes desde 3 y usa 5 por defecto.
 *
 * `insert()` y `remove()` comparten el descenso y la propagación por el
 * padre. La política se delega a `capacity()`, `min_keys()`,
 * `handle_overflow()` y `handle_underflow()` (Template Method).
 *
 * @warning Las operaciones de búsqueda y mutación bloquean el mutex del árbol
 * durante la llamada. Proteja con `mutex()` cualquier recorrido manual, la
 * creación y el uso de vistas inversas (también `rcall()`) y el acceso a
 * punteros o referencias devueltos por `find()`, `getRoot()` o `FirstThat()`
 * si hay otros hilos que mutan el árbol. `call()` y la serialización
 * mantienen el bloqueo durante el recorrido.
 * @warning Una mutación estructural puede invalidar iteradores, punteros y
 * referencias. Vuelva a obtenerlos después de modificar el árbol.
 * @warning El balanceo modifica páginas por etapas. Una excepción durante
 * una mutación o reinserción puede dejar cambios parciales.
 *
 * @tparam Traits Rasgos del árbol.
 * @tparam Mixins Mixins aplicados al contenedor.
 */
template <typename Traits, template <typename> typename... Mixins>
class BaseBTree : public GeneralContainer<Traits, Mixins...> {
public:
    using MySelf           = BaseBTree;
    using Parent           = GeneralContainer<Traits, Mixins...>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
    using Compare          = typename Traits::Compare;
    using Page             = typename Traits::Page;
    using PagePtr          = typename Page::PagePtr;
    using PageOwner        = typename Page::PageOwner;

    static constexpr Size MinOrder     = 3;   /**< Orden mínimo admitido (árbol 2-3). */
    static constexpr Size DefaultOrder = 5;   /**< Orden por defecto. */

protected:
    using Parent::m_mutex;
    using Buffer = BaseVector<VectorAscTraits<value_type>>;   /**< Búfer temporal `(valor, ref)` de `rebuild()`. */
    PageOwner m_root;                  /**< Raíz (`nullptr` si el árbol está vacío). */
    Size      m_order = DefaultOrder;  /**< Máximo de hijos por página. */
    Size      m_size{};                /**< Cantidad de claves almacenadas. */

    /** @brief Resultado del descenso: página, posición y si la clave fue encontrada. */
    struct Location {
        PagePtr page;    /**< Página donde está la clave o la hoja donde debería insertarse. */
        Index   pos;     /**< Posición (`lowerBound`) dentro de `page`. */
        bool    found;   /**< `true` si `page->key(pos)` es equivalente a la clave buscada. */
    };

    /**
     * @brief Descenso común desde la raíz guiado por `lowerBound`.
     * @param value Clave buscada.
     * @return Ubicación de la clave, o de la hoja y posición donde se insertaría.
     */
    Location locate(const value_type& value) const {
        PagePtr page = m_root.get();
        if (!page) return {nullptr, Index{}, false};
        while (true) {
            Index pos = page->lowerBound(value);
            if (page->matchesAt(pos, value)) return {page, pos, true};
            if (page->isLeaf())              return {page, pos, false};
            page = page->child(pos);
        }
    }

    /**
     * @brief Crea una página vacía con la capacidad que indica `capacity(isRoot)`.
     * @param isRoot `true` si la página será la raíz.
     * @return Página propia sin claves ni hijos.
     */
    PageOwner make_page(bool isRoot) const { return std::make_unique<Page>(capacity(isRoot)); }

    // ------------------------------------------------------------ ganchos (Template Method)

    /**
     * @brief Capacidad (máximo de claves) de una página nueva.
     * @param isRoot `true` si la página será la raíz (el árbol clásico no distingue).
     * @return `m_order - 1`.
     */
    virtual Size capacity([[maybe_unused]] bool isRoot) const { return m_order - 1; }

    /**
     * @brief Mínimo de claves que debe conservar `page`.
     * @param page Página consultada.
     * @return 1 para la raíz de un árbol no vacío; `⌈m/2⌉ - 1` para el resto.
     */
    virtual Size min_keys(const Page& page) const {
        if (page.isRoot()) return 1;
        return (m_order - 1) / 2;
    }

    /**
     * @brief Corrige una página en overflow. Precondición: `page` no es raíz.
     *
     * Clásico: división 1→2 subiendo la mediana al padre. Puede dejar al
     * padre en overflow; la base se encarga de seguir propagando.
     * @param page Página con `isOverflow() == true`.
     */
    virtual void handle_overflow(PagePtr page) { split_in_two(page); }

    /**
     * @brief Corrige una página con menos claves que `min_keys()`. Precondición: `page` no es raíz.
     *
     * Clásico: pide prestada una clave al hermano izquierdo o derecho si le
     * sobran; si no, fusiona con uno de ellos (puede dejar al padre en underflow).
     * @param page Página en underflow (puede ser destruida si se fusiona con su hermano izquierdo).
     */
    virtual void handle_underflow(PagePtr page) {
        PagePtr parent = page->parent();
        Index   pos    = *page->positionInParent();
        PagePtr left   = pos > 0 ? parent->child(pos - 1) : nullptr;
        PagePtr right  = pos + 1 < parent->childCount() ? parent->child(pos + 1) : nullptr;
        if      (left  && left->keyCount()  > min_keys(*left))  borrow_from_left(parent, pos);
        else if (right && right->keyCount() > min_keys(*right)) borrow_from_right(parent, pos);
        else if (left)                                          merge_children(parent, pos - 1);
        else                                                    merge_children(parent, pos);
    }

    // ------------------------------------------------------------ operaciones reutilizables

    /**
     * @brief Divide `page` en dos subiendo su clave mediana al padre.
     *
     * Las claves `[0, mid)` quedan en `page`, la mediana sube al padre y
     * `(mid, n)` pasan a una hermana nueva insertada a la derecha (con sus hijos).
     * @param page Página a dividir (debe tener padre).
     */
    void split_in_two(PagePtr page) {
        PagePtr parent = page->parent();
        Index   pos    = *page->positionInParent();
        Index   mid    = page->keyCount() / 2;
        PageOwner right = make_page(false);
        for (Index i = mid + 1; i < page->keyCount(); ++i)
            right->insertKey(right->keyCount(), page->key(i));
        if (!page->isLeaf())
            while (page->childCount() > mid + 1)
                right->insertChild(right->childCount(), page->removeChild(mid + 1));
        while (page->keyCount() > mid + 1) page->eraseKey(page->keyCount() - 1);
        parent->insertKey(pos, page->eraseKey(mid));
        parent->insertChild(pos + 1, std::move(right));
    }

    /**
     * @brief Rotación a la derecha: el hijo `pos` recibe el separador `pos - 1` y el
     *        hermano izquierdo aporta la nueva clave separadora y, si es interno, cede su último hijo al receptor.
     * @param parent Página padre.
     * @param pos Posición del hijo receptor (`pos > 0`).
     */
    void borrow_from_left(PagePtr parent, Index pos) {
        PagePtr donor    = parent->child(pos - 1);
        PagePtr receiver = parent->child(pos);
        receiver->insertKey(0, parent->key(pos - 1));
        parent->key(pos - 1) = donor->eraseKey(donor->keyCount() - 1);
        if (!donor->isLeaf())
            receiver->insertChild(0, donor->removeChild(donor->childCount() - 1));
    }

    /**
     * @brief Rotación a la izquierda: el hijo `pos` recibe el separador `pos` y el
     *        hermano derecho aporta la nueva clave separadora y, si es interno, cede su primer hijo al receptor.
     * @param parent Página padre.
     * @param pos Posición del hijo receptor (`pos + 1 < parent->childCount()`).
     */
    void borrow_from_right(PagePtr parent, Index pos) {
        PagePtr donor    = parent->child(pos + 1);
        PagePtr receiver = parent->child(pos);
        receiver->insertKey(receiver->keyCount(), parent->key(pos));
        parent->key(pos) = donor->eraseKey(0);
        if (!donor->isLeaf())
            receiver->insertChild(receiver->childCount(), donor->removeChild(0));
    }

    /**
     * @brief Fusiona los hijos `pos` y `pos + 1` del padre bajando el separador `pos` (2→1).
     *
     * El hijo `pos + 1` se destruye; el padre pierde una clave y un hijo.
     * @param parent Página padre.
     * @param pos Posición del hijo izquierdo de la fusión.
     */
    void merge_children(PagePtr parent, Index pos) {
        PagePtr   left  = parent->child(pos);
        PageOwner right = parent->removeChild(pos + 1);
        left->insertKey(left->keyCount(), parent->eraseKey(pos));
        for (Index i{}; i < right->keyCount(); ++i)
            left->insertKey(left->keyCount(), right->key(i));
        while (!right->isLeaf())
            left->insertChild(left->childCount(), right->removeChild(0));
    }

    /**
     * @brief Reúne en una página temporal los hijos `[first, first + count)` del padre y sus `count - 1` separadores.
     *
     * Los hijos se extraen del padre y se destruyen al terminar; el padre pierde
     * `count` hijos y `count - 1` claves. Solo usa primitivas de `BTreePage`,
     * por lo que cada nieto queda enlazado a la página temporal.
     * @param parent Página padre.
     * @param first Posición del primer hijo a reunir.
     * @param count Cantidad de hijos consecutivos (`>= 1`).
     * @return Página temporal sin padre con todas las claves en orden y, si los hijos eran internos, todos los nietos.
     */
    PageOwner gather_children(PagePtr parent, Index first, Size count) {
        PageOwner pool = std::make_unique<Page>(capacity(true));
        for (Index k{}; k < count; ++k) {
            PageOwner child = parent->removeChild(first);
            for (Index i{}; i < child->keyCount(); ++i)
                pool->insertKey(pool->keyCount(), child->key(i));
            while (!child->isLeaf())
                pool->insertChild(pool->childCount(), child->removeChild(0));
            if (k + 1 < count) pool->insertKey(pool->keyCount(), parent->eraseKey(first));
        }
        return pool;
    }

    /**
     * @brief Reparte equitativamente `count` hijos consecutivos (con sus separadores) en `outCount` páginas nuevas.
     *
     * De las `K` claves reunidas, `outCount - 1` quedan como separadores
     * en el padre. Las demás se reparten con diferencia máxima de una
     * clave. El llamador debe comprobar mínimos y capacidades.
     * @param parent Página padre.
     * @param first Posición del primer hijo.
     * @param count Cantidad de hijos de entrada (`>= 1`).
     * @param outCount Cantidad de páginas de salida (`>= 1`).
     */
    void redistribute_children(PagePtr parent, Index first, Size count, Size outCount) {
        PageOwner pool   = gather_children(parent, first, count);
        bool      inner  = !pool->isLeaf();
        Size      total  = pool->keyCount() - (outCount - 1);
        Size      share  = total / outCount;
        Size      extra  = total % outCount;
        Index     cursor{};
        for (Index j{}; j < outCount; ++j) {
            Size      keys = j < extra ? share + 1 : share;
            PageOwner page = make_page(false);
            for (Index i{}; i < keys; ++i)
                page->insertKey(page->keyCount(), pool->key(cursor++));
            if (inner)
                for (Index i{}; i <= keys; ++i)
                    page->insertChild(page->childCount(), pool->removeChild(0));
            parent->insertChild(first + j, std::move(page));
            if (j + 1 < outCount) parent->insertKey(first + j, pool->key(cursor++));
        }
    }

    /**
     * @brief La página absorbe a todos sus hijos: sus claves y separadores pasan a ella y los nietos pasan a ser sus hijos.
     *
     * Reduce en 1 la altura del subárbol (en la raíz, la del árbol). La
     * página conserva su identidad y su capacidad; el llamador garantiza que
     * el total de claves cabe en ella.
     * @param page Página interna cuyas claves son exactamente los `childCount() - 1` separadores de sus hijos.
     */
    void absorb_children(PagePtr page) {
        PageOwner pool = gather_children(page, 0, page->childCount());
        for (Index i{}; i < pool->keyCount(); ++i)
            page->insertKey(page->keyCount(), pool->key(i));
        while (!pool->isLeaf())
            page->insertChild(page->childCount(), pool->removeChild(0));
    }

    // ------------------------------------------------------------ algoritmo común

    /**
     * @brief Propaga el overflow desde `page` hacia la raíz, haciendo crecer el árbol si hace falta.
     * @param page Página donde se insertó (o que recibió un separador).
     */
    void propagate_overflow(PagePtr page) {
        while (page && page->isOverflow()) {
            if (page->isRoot()) grow_root();
            PagePtr parent = page->parent();
            handle_overflow(page);
            page = parent;
        }
    }

    /** @brief Crea una raíz nueva sin claves cuyo único hijo es la raíz actual. */
    void grow_root() {
        PageOwner newRoot = make_page(true);
        PageOwner oldRoot = std::move(m_root);
        oldRoot->setCapacity(capacity(false));
        newRoot->insertChild(0, std::move(oldRoot));
        m_root = std::move(newRoot);
    }

    /**
     * @brief Propaga el underflow desde `page` hacia la raíz y luego contrae la raíz si quedó vacía.
     * @param page Hoja de la que se eliminó una clave.
     */
    void propagate_underflow(PagePtr page) {
        while (!page->isRoot() && page->keyCount() < min_keys(*page)) {
            PagePtr parent = page->parent();
            handle_underflow(page);
            page = parent;
        }
        shrink_root();
    }

    /** @brief Si la raíz quedó sin claves, la reemplaza por su único hijo (o vacía el árbol). */
    void shrink_root() {
        if (!m_root || m_root->keyCount() > 0) return;
        if (m_root->isLeaf()) { m_root.reset(); return; }
        PageOwner child = m_root->removeChild(0);
        child->setCapacity(capacity(true));
        m_root = std::move(child);
        propagate_overflow(m_root.get());   // por si la capacidad de raíz es menor (variantes)
    }

public:
    /**
     * @brief Construye un árbol vacío.
     * @param order Máximo de hijos por página (`>= MinOrder`).
     * @throws std::invalid_argument Si `order < MinOrder`.
     */
    explicit BaseBTree(Size order = DefaultOrder) : Parent(), m_order(order) {
        if (order < MinOrder) throw std::invalid_argument("El orden del arbol B debe ser al menos 3");
    }

    /**
     * @brief Constructor de copia estructural profunda.
     * @param other Árbol origen del mismo tipo dinámico.
     * @see operator=(const BaseBTree&)
     */
    BaseBTree(const BaseBTree& other) : Parent::Core(), Parent() { *this = other; }
    /**
     * @brief Constructor de movimiento: el origen queda vacío y reutilizable.
     * @param other Árbol origen.
     */
    BaseBTree(BaseBTree&& other) noexcept : Parent() { *this = std::move(other); }

    /** @brief Destructor seguro: `clear()` bajo bloqueo; las páginas se liberan por `unique_ptr`. */
    virtual ~BaseBTree() { clear(); }

    /**
     * @brief Asignación por copia estructural (misma forma) con `BTreePage::clone()`.
     *
     * Clona las páginas antes de sustituir la raíz. Si la clonación falla,
     * el destino conserva su estado. La copia recorre las claves de cada página.
     *
     * @warning Copie o mueva solo entre árboles del mismo tipo dinámico.
     * La operación conserva las capacidades del origen, que pueden ser
     * incompatibles con la política del destino. Para convertir entre
     * variantes, reinserte los pares de clave y referencia.
     * @param other Árbol origen.
     * @return `*this`.
     */
    BaseBTree& operator=(const BaseBTree& other) {
        if (this == &other) return *this;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        PageOwner copy = other.m_root ? other.m_root->clone() : nullptr;
        m_root  = std::move(copy);
        m_order = other.m_order;
        m_size  = other.m_size;
        return *this;
    }

    /**
     * @brief Asignación por movimiento: transfiere raíz, orden y tamaño.
     *
     * El origen conserva su orden, queda vacío (`size() == 0`) y puede seguir usándose.
     * @param other Árbol origen del mismo tipo dinámico.
     * @return `*this`.
     */
    BaseBTree& operator=(BaseBTree&& other) noexcept {
        this->move_from(other, [&]{
            m_root  = std::exchange(other.m_root, nullptr);
            m_order = other.m_order;
            m_size  = std::exchange(other.m_size, Size{});
        });
        return *this;
    }

    /**
     * @brief Inserta `(value, ref)` si no existe una clave equivalente.
     * @param value Clave a insertar.
     * @param ref Referencia asociada.
     * @return `true` si se insertó; `false` si era duplicada.
     */
    bool insert(const value_type& value, Ref ref = 0) {
        std::lock_guard lock(m_mutex);
        if (!m_root) m_root = make_page(true);
        Location loc = locate(value);
        if (loc.found) return false;
        loc.page->insertKey(loc.pos, value, ref);
        ++m_size;
        propagate_overflow(loc.page);
        return true;
    }

    /**
     * @brief Inserta delegando en `insert()` (los duplicados se ignoran).
     * @param value Clave a insertar.
     * @param ref Referencia asociada.
     */
    void push_back(const value_type& value, Ref ref = 0) override { insert(value, ref); }

    /**
     * @brief Elimina la clave equivalente a `value`.
     *
     * Si está en una página interna se reemplaza por su predecesor (la última
     * clave de la hoja más a la derecha del subárbol izquierdo) y se elimina
     * esa clave de la hoja.
     * @param value Clave a eliminar.
     * @return `true` si existía y fue eliminada.
     */
    bool remove(const value_type& value) {
        std::lock_guard lock(m_mutex);
        Location loc = locate(value);
        if (!loc.found) return false;
        PagePtr page = loc.page;
        if (page->isLeaf()) {
            page->eraseKey(loc.pos);
        } else {
            PagePtr leaf = page->child(loc.pos);
            while (!leaf->isLeaf()) leaf = leaf->child(leaf->childCount() - 1);
            page->key(loc.pos) = leaf->eraseKey(leaf->keyCount() - 1);
            page = leaf;
        }
        --m_size;
        propagate_underflow(page);
        return true;
    }

    /**
     * @brief Busca una clave equivalente a `value` mediante búsqueda binaria por página.
     * @note Para altura `h`, realiza O(h log m) comparaciones.
     * @param value Clave buscada.
     * @return Puntero mutable al valor almacenado o `nullptr` si no existe.
     */
    value_type* find(const value_type& value) {
        std::lock_guard lock(m_mutex);
        Location loc = locate(value);
        return loc.found ? &loc.page->key(loc.pos).value() : nullptr;
    }

    /**
     * @brief Igual que `find()` pero de solo lectura.
     * @param value Clave buscada.
     * @return Puntero constante al valor o `nullptr` si no existe.
     */
    const value_type* find(const value_type& value) const {
        std::lock_guard lock(m_mutex);
        Location loc = locate(value);
        return loc.found ? &loc.page->key(loc.pos).value() : nullptr;
    }

    /** @brief Cantidad de claves almacenadas. @return Cantidad, leída bajo bloqueo. */
    Size size() const {
        std::lock_guard lock(m_mutex);
        return m_size;
    }

    /** @brief Orden del árbol. @return Máximo de hijos por página. */
    Size order() const {
        std::lock_guard lock(m_mutex);
        return m_order;
    }

    /** @brief Altura en páginas. @return 0 si está vacío, 1 si solo hay raíz. */
    Size height() const {
        std::lock_guard lock(m_mutex);
        Size h{};
        for (PagePtr page = m_root.get(); page; page = page->isLeaf() ? nullptr : page->child(0)) ++h;
        return h;
    }

    /** @brief Libera todas las páginas. */
    void clear() override {
        std::lock_guard lock(m_mutex);
        m_root.reset();
        m_size = 0;
    }

    /**
     * @brief Reconstruye el árbol reinsertando sus pares `(valor, ref)`.
     *
     * Tras `ApplyFunction()` recupera el orden por `Compare`. Si hay claves
     * equivalentes, conserva la primera del recorrido previo con su `Ref`.
     * @note La reinserción ocurre después de vaciar el árbol e invalida
     * los iteradores, punteros y referencias a sus elementos. Una excepción
     * puede dejar la reconstrucción incompleta.
     */
    void rebuild() override {
        std::lock_guard lock(m_mutex);
        Buffer items;
        items.reserve(m_size);
        for (const Node& node : *this) items.push_back(node.getValue(), node.getRef());
        clear();
        for (const Node& node : items) insert(node.getValue(), node.getRef());
    }

    /** @brief Raíz de solo lectura (no bloquea, ver contrato de concurrencia). @return Raíz o `nullptr` si está vacío. */
    const Page* getRoot() const { return m_root.get(); }

    /**
     * @brief Consulta pública (NVI) del gancho virtual `min_keys()`.
     * @param page Página consultada.
     * @return Mínimo de claves que la política del árbol exige a `page`.
     */
    Size minKeysFor(const Page& page) const { return min_keys(page); }
    /**
     * @brief Consulta pública (NVI) del gancho virtual `capacity()`.
     * @param isRoot `true` para la raíz.
     * @return Capacidad de una página raíz o no raíz.
     */
    Size capacityFor(bool isRoot) const { return capacity(isRoot); }

    /** @brief Inicio del recorrido in-order (no bloquea). @return Iterador a la primera clave. */
    ForwardIterator  begin()  const override { return ForwardIterator(m_root.get()); }
    /** @brief Fin del recorrido in-order. @return Iterador `end()` (página nula). */
    ForwardIterator  end()    const override { return ForwardIterator(nullptr); }
    /** @brief Inicio del recorrido inverso (no bloquea). @return Iterador a la última clave. */
    BackwardIterator rbegin() const { return BackwardIterator(m_root.get()); }
    /** @brief Fin del recorrido inverso. @return Iterador `end()` (página nula). */
    BackwardIterator rend()   const { return BackwardIterator(nullptr); }
};

#endif // __BTREE_H__
