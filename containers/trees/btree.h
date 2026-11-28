/**
 * @file btree.h
 * @ingroup trees
 * @brief Árbol B (`BaseBTree` / `BTree`), su página (`BTreePage`), iterador (`BTreeIterator`) y rasgos.
 *
 * La página guarda claves y enlaces; las políticas de balanceo las decide el
 * árbol mediante ganchos virtuales (Template Method), que variantes como B* redefinen.
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

/// Rasgos del `BaseVector` que guarda las claves de una página.
template <typename T, typename Compare>
struct BTreeKeyTraits : public DefaultTraits<T, Compare>, public VectorTraits<T> {};

/**
 * @class BTreePage
 * @ingroup trees
 * @brief Página con claves ordenadas, hijos propios (`unique_ptr`) y puntero al padre.
 *
 * Estable: una clave menos que hijos. Las primitivas admiten estados intermedios.
 */
template <typename T, typename Compare>
class BTreePage {
public:
    using value_type = T;
    using MySelf     = BTreePage<T, Compare>;
    using KeyTraits  = BTreeKeyTraits<T, Compare>;
    using Keys       = BaseVector<KeyTraits>;
    using Node       = typename Keys::Node;      ///< Valor + `Ref`.
    using PagePtr    = MySelf*;                  ///< No propietario.
    using PageOwner  = std::unique_ptr<MySelf>;  ///< Propietario.
    using Children   = std::vector<PageOwner>;

private:
    Keys     m_keys;               ///< Claves ordenadas.
    Children m_children;           ///< Hijos (vacío en hojas).
    PagePtr  m_parent = nullptr;   ///< `nullptr` en la raíz.
    Size     m_capacity;           ///< Máximo de claves en estado estable.

    /// @throws std::out_of_range Si `pos >= limit`.
    static void checkRange(Index pos, Size limit, const char* what) {
        if (pos >= limit) throw std::out_of_range(what);
    }

public:
    /// Página hoja vacía. @throws std::invalid_argument Si `capacity == 0`.
    explicit BTreePage(Size capacity) : m_capacity(capacity) {
        if (capacity == 0) throw std::invalid_argument("La capacidad de la pagina debe ser positiva");
        m_keys.reserve(capacity + 1);
    }

    virtual ~BTreePage() = default;

    BTreePage(const BTreePage&)            = delete;
    BTreePage& operator=(const BTreePage&) = delete;
    BTreePage(BTreePage&&)                 = delete;
    BTreePage& operator=(BTreePage&&)      = delete;

    // ---------------------------------------------------------------- consultas

    Size capacity()   const { return m_capacity; }
    Size keyCount()   const { return m_keys.size(); }
    Size childCount() const { return m_children.size(); }
    bool isLeaf()     const { return m_children.empty(); }
    bool isRoot()     const { return m_parent == nullptr; }
    bool isFull()     const { return keyCount() >= m_capacity; }
    /// Estado transitorio durante la inserción.
    bool isOverflow() const { return keyCount() > m_capacity; }

    PagePtr parent() const { return m_parent; }

    /// Cambia la capacidad sin reorganizar claves. @throws std::invalid_argument Si es 0.
    void setCapacity(Size capacity) {
        if (capacity == 0) throw std::invalid_argument("La capacidad de la pagina debe ser positiva");
        m_capacity = capacity;
    }

    /// Clave en `pos`. @throws std::out_of_range
    const Node& key(Index pos) const {
        checkRange(pos, keyCount(), "Posicion de clave fuera de rango");
        return m_keys[pos];
    }

    /// Clave mutable en `pos`. @throws std::out_of_range
    Node& key(Index pos) {
        checkRange(pos, keyCount(), "Posicion de clave fuera de rango");
        return m_keys[pos];
    }

    /// Hijo en `pos`. @throws std::out_of_range
    PagePtr child(Index pos) const {
        checkRange(pos, childCount(), "Posicion de hijo fuera de rango");
        return m_children[pos].get();
    }

    // ---------------------------------------------------------------- búsqueda

    /// Primera posición cuya clave no precede a `value`; también es el hijo por el que descender.
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

    /// `true` si la clave en `pos` es equivalente a `value`.
    bool matchesAt(Index pos, const value_type& value) const {
        return pos < keyCount() && !Compare{}(value, m_keys[pos].getValue())
                                && !Compare{}(m_keys[pos].getValue(), value);
    }

    /// Busca `value` solo en esta página.
    std::optional<Index> find(const value_type& value) const {
        Index pos = lowerBound(value);
        if (matchesAt(pos, value)) return pos;
        return std::nullopt;
    }

    // ---------------------------------------------------------------- claves

    /// Inserta `node` en `pos` desplazando el resto. @throws std::out_of_range
    void insertKey(Index pos, const Node& node) {
        checkRange(pos, keyCount() + 1, "Posicion de insercion de clave fuera de rango");
        m_keys.push_back(node.getValue(), node.getRef());
        for (Index i = keyCount() - 1; i > pos; --i)
            m_keys[i] = m_keys[i - 1];
        m_keys[pos] = node;
    }

    void insertKey(Index pos, const value_type& value, Ref ref) { insertKey(pos, Node(value, ref)); }

    /// Inserta en orden y retorna la posición.
    Index insertSorted(const value_type& value, Ref ref) {
        Index pos = lowerBound(value);
        insertKey(pos, value, ref);
        return pos;
    }

    /// Elimina y retorna la clave en `pos`. @throws std::out_of_range
    Node eraseKey(Index pos) {
        checkRange(pos, keyCount(), "Posicion de eliminacion de clave fuera de rango");
        Node removed = m_keys[pos];
        for (Index i = pos + 1; i < keyCount(); ++i)
            m_keys[i - 1] = m_keys[i];
        m_keys.pop_back();
        return removed;
    }

    // ---------------------------------------------------------------- hijos

    /// Inserta un hijo en `pos` y lo enlaza. @throws std::out_of_range, std::invalid_argument si es nulo.
    PagePtr insertChild(Index pos, PageOwner page) {
        checkRange(pos, childCount() + 1, "Posicion de insercion de hijo fuera de rango");
        if (!page) throw std::invalid_argument("No se puede insertar un hijo nulo");
        page->m_parent = this;
        PagePtr raw = page.get();
        m_children.insert(m_children.begin() + pos, std::move(page));
        return raw;
    }

    /// Extrae el hijo en `pos` y lo desenlaza. @throws std::out_of_range
    PageOwner removeChild(Index pos) {
        checkRange(pos, childCount(), "Posicion de eliminacion de hijo fuera de rango");
        PageOwner page = std::move(m_children[pos]);
        m_children.erase(m_children.begin() + pos);
        page->m_parent = nullptr;
        return page;
    }

    /// Posición de `page` entre los hijos, o `nullopt`.
    std::optional<Index> indexOfChild(const MySelf* page) const {
        for (Index i{}; i < childCount(); ++i)
            if (m_children[i].get() == page) return i;
        return std::nullopt;
    }

    /// Posición en el padre, o `nullopt` si es raíz.
    std::optional<Index> positionInParent() const {
        if (!m_parent) return std::nullopt;
        return m_parent->indexOfChild(this);
    }

    // ---------------------------------------------------------------- copia y validación

    /// Copia profunda del subárbol (la raíz de la copia no tiene padre).
    PageOwner clone() const {
        auto copy = std::make_unique<MySelf>(m_capacity);
        copy->m_keys = m_keys;
        for (const PageOwner& c : m_children)
            copy->insertChild(copy->childCount(), c->clone());
        return copy;
    }

    /// Verifica orden de claves, número de hijos y enlaces al padre (no la ocupación mínima).
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
 * @brief Iterador in-order sin pila (espacio O(1)); sube usando `parent()`.
 * @tparam Dir `Asc` sigue a `Compare`; `Desc` lo invierte.
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

    PagePtr m_page = nullptr;   ///< `nullptr` en `end()`.
    Index   m_pos{};            ///< Posición en `m_page`.

    /// Actualiza `m_ptr` desde `(m_page, m_pos)`.
    void sync() {
        if (!m_page) m_pos = 0;
        m_ptr = m_page ? &m_page->key(m_pos) : nullptr;
    }

    /// Va a la primera clave (según `Dir`) del subárbol `page`.
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

    /// Sube hasta un ancestro con clave pendiente; si no hay, queda en `end()`.
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
    /// Iterador en la primera clave; `end()` si `root` es nulo o vacío.
    explicit BTreeIterator(PagePtr root = nullptr) : Parent(nullptr) {
        if (root && root->keyCount() > 0) descend(root);
        sync();
    }

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

    PagePtr page()     const { return m_page; }
    Index   position() const { return m_pos; }

    /// Misma página y posición.
    friend bool operator==(const MySelf& a, const MySelf& b) {
        return a.m_page == b.m_page && a.m_pos == b.m_pos;
    }
    friend bool operator!=(const MySelf& a, const MySelf& b) { return !(a == b); }
};

/// Nodo, página e iteradores del árbol B.
template <typename T, typename Cmp>
struct BTreeTraits {
    using Node             = GeneralNode<T>;
    using Page             = BTreePage<T, Cmp>;
    using ForwardIterator  = BTreeIterator<T, Cmp, Direction::Asc>;
    using BackwardIterator = BTreeIterator<T, Cmp, Direction::Desc>;
};

/// Árbol B ascendente (`std::less<T>`).
template <typename T>
struct BTreeAscTraits : public AscendingTraits<T>,
                        public BTreeTraits<T, typename AscendingTraits<T>::Compare> {};

/// Árbol B descendente (`std::greater<T>`).
template <typename T>
struct BTreeDescTraits : public DescendingTraits<T>,
                         public BTreeTraits<T, typename DescendingTraits<T>::Compare> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseBTree;

/// Árbol B estándar con todos los mixins (incluido `Drawable`).
template <typename Traits>
using BTree = BaseBTree<Traits, Callable, Applicable, Searchable, Reversible, Printable, Readable, Drawable>;

/**
 * @class BaseBTree
 * @ingroup trees
 * @brief Árbol B de claves únicas sobre `GeneralContainer`.
 *
 * Orden `m` (>= 3, por defecto 5): máximo `m - 1` claves, mínimo `⌊(m - 1)/2⌋`.
 * La política se delega a `capacity()`, `min_keys()`, `handle_overflow()` y
 * `handle_underflow()`.
 *
 * @warning Búsqueda y mutación bloquean el mutex; recorridos manuales, vistas
 * inversas y punteros de `find()`/`getRoot()`/`FirstThat()` deben protegerse con `mutex()`.
 * @warning Mutar el árbol invalida iteradores y punteros. Una excepción a
 * mitad de una mutación puede dejar cambios parciales.
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

    static constexpr Size MinOrder     = 3;   ///< Árbol 2-3.
    static constexpr Size DefaultOrder = 5;

protected:
    using Parent::m_mutex;
    using Buffer = BaseVector<VectorAscTraits<value_type>>;   ///< Búfer de `rebuild()`.
    PageOwner m_root;                  ///< `nullptr` si está vacío.
    Size      m_order = DefaultOrder;  ///< Máximo de hijos por página.
    Size      m_size{};                ///< Cantidad de claves.

    /// Resultado de `locate()`.
    struct Location {
        PagePtr page;    ///< Página de la clave o hoja de inserción.
        Index   pos;     ///< `lowerBound` en `page`.
        bool    found;
    };

    /// Desciende desde la raíz hasta la clave o la hoja donde iría.
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

    /// Página vacía con `capacity(isRoot)`.
    PageOwner make_page(bool isRoot) const { return std::make_unique<Page>(capacity(isRoot)); }

    // ------------------------------------------------------------ ganchos (Template Method)

    /// Máximo de claves: `m - 1`.
    virtual Size capacity([[maybe_unused]] bool isRoot) const { return m_order - 1; }

    /// Mínimo de claves: 1 en la raíz, `⌊(m - 1)/2⌋` en el resto.
    virtual Size min_keys(const Page& page) const {
        if (page.isRoot()) return 1;
        return (m_order - 1) / 2;
    }

    /// Overflow (página no raíz): división 1→2.
    virtual void handle_overflow(PagePtr page) { split_in_two(page); }

    /// Underflow (página no raíz): préstamo de un hermano o fusión.
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

    /// Divide `page` en dos y sube la mediana al padre.
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

    /// Rotación: el hijo `pos` recibe una clave (y un hijo) del hermano izquierdo.
    void borrow_from_left(PagePtr parent, Index pos) {
        PagePtr donor    = parent->child(pos - 1);
        PagePtr receiver = parent->child(pos);
        receiver->insertKey(0, parent->key(pos - 1));
        parent->key(pos - 1) = donor->eraseKey(donor->keyCount() - 1);
        if (!donor->isLeaf())
            receiver->insertChild(0, donor->removeChild(donor->childCount() - 1));
    }

    /// Rotación: el hijo `pos` recibe una clave (y un hijo) del hermano derecho.
    void borrow_from_right(PagePtr parent, Index pos) {
        PagePtr donor    = parent->child(pos + 1);
        PagePtr receiver = parent->child(pos);
        receiver->insertKey(receiver->keyCount(), parent->key(pos));
        parent->key(pos) = donor->eraseKey(0);
        if (!donor->isLeaf())
            receiver->insertChild(receiver->childCount(), donor->removeChild(0));
    }

    /// Fusiona los hijos `pos` y `pos + 1` bajando el separador (2→1).
    void merge_children(PagePtr parent, Index pos) {
        PagePtr   left  = parent->child(pos);
        PageOwner right = parent->removeChild(pos + 1);
        left->insertKey(left->keyCount(), parent->eraseKey(pos));
        for (Index i{}; i < right->keyCount(); ++i)
            left->insertKey(left->keyCount(), right->key(i));
        while (!right->isLeaf())
            left->insertChild(left->childCount(), right->removeChild(0));
    }

    /// Extrae `count` hijos desde `first` (con sus separadores) y los reúne en una página temporal.
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

    /// Reparte `count` hijos en `outCount` páginas equilibradas. El llamador valida capacidades.
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

    /// `page` absorbe a todos sus hijos (reduce la altura). El llamador garantiza que caben.
    void absorb_children(PagePtr page) {
        PageOwner pool = gather_children(page, 0, page->childCount());
        for (Index i{}; i < pool->keyCount(); ++i)
            page->insertKey(page->keyCount(), pool->key(i));
        while (!pool->isLeaf())
            page->insertChild(page->childCount(), pool->removeChild(0));
    }

    // ------------------------------------------------------------ algoritmo común

    /// Propaga el overflow hacia la raíz, creciendo el árbol si hace falta.
    void propagate_overflow(PagePtr page) {
        while (page && page->isOverflow()) {
            if (page->isRoot()) grow_root();
            PagePtr parent = page->parent();
            handle_overflow(page);
            page = parent;
        }
    }

    /// Nueva raíz vacía con la raíz actual como único hijo.
    void grow_root() {
        PageOwner newRoot = make_page(true);
        PageOwner oldRoot = std::move(m_root);
        oldRoot->setCapacity(capacity(false));
        newRoot->insertChild(0, std::move(oldRoot));
        m_root = std::move(newRoot);
    }

    /// Propaga el underflow hacia la raíz y luego la contrae.
    void propagate_underflow(PagePtr page) {
        while (!page->isRoot() && page->keyCount() < min_keys(*page)) {
            PagePtr parent = page->parent();
            handle_underflow(page);
            page = parent;
        }
        shrink_root();
    }

    /// Si la raíz quedó vacía, la reemplaza por su único hijo.
    void shrink_root() {
        if (!m_root || m_root->keyCount() > 0) return;
        if (m_root->isLeaf()) { m_root.reset(); return; }
        PageOwner child = m_root->removeChild(0);
        child->setCapacity(capacity(true));
        m_root = std::move(child);
        propagate_overflow(m_root.get());   // por si la capacidad de raíz es menor (variantes)
    }

public:
    /// @throws std::invalid_argument Si `order < MinOrder`.
    explicit BaseBTree(Size order = DefaultOrder) : Parent(), m_order(order) {
        if (order < MinOrder) throw std::invalid_argument("El orden del arbol B debe ser al menos 3");
    }

    BaseBTree(const BaseBTree& other) : Parent::Core(), Parent() { *this = other; }
    /// El origen queda vacío y reutilizable.
    BaseBTree(BaseBTree&& other) noexcept : Parent() { *this = std::move(other); }

    virtual ~BaseBTree() { clear(); }

    /**
     * @brief Copia profunda (misma forma). Si falla, el destino no cambia.
     * @warning Solo entre árboles del mismo tipo; conserva las capacidades del origen.
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

    /// Transfiere el contenido; el origen queda vacío y reutilizable.
    BaseBTree& operator=(BaseBTree&& other) noexcept {
        this->move_from(other, [&]{
            m_root  = std::exchange(other.m_root, nullptr);
            m_order = other.m_order;
            m_size  = std::exchange(other.m_size, Size{});
        });
        return *this;
    }

    /// Inserta si no existe. @return `false` si era duplicada.
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

    /// Igual que `insert()` (ignora duplicados).
    void push_back(const value_type& value, Ref ref = 0) override { insert(value, ref); }

    /// Elimina `value`; en página interna lo reemplaza por su predecesor. @return `true` si existía.
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

    /// Busca `value` en O(h log m). @return Puntero al valor o `nullptr`.
    value_type* find(const value_type& value) {
        std::lock_guard lock(m_mutex);
        Location loc = locate(value);
        return loc.found ? &loc.page->key(loc.pos).value() : nullptr;
    }

    const value_type* find(const value_type& value) const {
        std::lock_guard lock(m_mutex);
        Location loc = locate(value);
        return loc.found ? &loc.page->key(loc.pos).value() : nullptr;
    }

    Size size() const {
        std::lock_guard lock(m_mutex);
        return m_size;
    }

    Size order() const {
        std::lock_guard lock(m_mutex);
        return m_order;
    }

    /// Altura en páginas (0 si está vacío).
    Size height() const {
        std::lock_guard lock(m_mutex);
        Size h{};
        for (PagePtr page = m_root.get(); page; page = page->isLeaf() ? nullptr : page->child(0)) ++h;
        return h;
    }

    void clear() override {
        std::lock_guard lock(m_mutex);
        m_root.reset();
        m_size = 0;
    }

    /**
     * @brief Reinserta todas las claves (p. ej. tras `ApplyFunction()`).
     * Ante duplicados conserva la primera. Invalida iteradores y punteros.
     */
    void rebuild() override {
        std::lock_guard lock(m_mutex);
        Buffer items;
        items.reserve(m_size);
        for (const Node& node : *this) items.push_back(node.getValue(), node.getRef());
        clear();
        for (const Node& node : items) insert(node.getValue(), node.getRef());
    }

    /// Raíz de solo lectura (no bloquea).
    const Page* getRoot() const { return m_root.get(); }

    /// Acceso público a `min_keys()`.
    Size minKeysFor(const Page& page) const { return min_keys(page); }
    /// Acceso público a `capacity()`.
    Size capacityFor(bool isRoot) const { return capacity(isRoot); }

    // Recorridos (no bloquean).
    ForwardIterator  begin()  const override { return ForwardIterator(m_root.get()); }
    ForwardIterator  end()    const override { return ForwardIterator(nullptr); }
    BackwardIterator rbegin() const { return BackwardIterator(m_root.get()); }
    BackwardIterator rend()   const { return BackwardIterator(nullptr); }
};

#endif // __BTREE_H__
