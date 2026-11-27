#ifndef CONTAINERS_BTREE_H
#define CONTAINERS_BTREE_H

#include "BTreePage.h"
#include "../core/traits.h"
#include "../core/traversalops.h"
#include "binarytreeiterator.h"
#include <initializer_list>
#include <mutex>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

/**
 * Árbol B* en memoria: las páginas se mantienen llenas al menos en 2/3 y, al desbordarse, dos páginas se dividen en tres.
 * La raíz admite `2 * orden + 1` claves y el resto `orden`.
 * @tparam Traits Tipos de valor, referencia, comparador, página e iteradores, y el orden por defecto.
 * @tparam Ops Mixins de recorrido, impresión y lectura.
 */
template <typename Traits, typename Ops = FullTraversalOps>
    requires BTreeTraitsConcept<Traits> && BTreePageConcept<typename Traits::Page>
class BTree : public Ops, public TreeOrderOps {
    friend class TreeOrderOps;

public:
    using value_type = typename Traits::value_type;
    using ref_type = typename Traits::ref_type;
    using Compare = typename Traits::Compare;
    using Page = typename Traits::Page;
    using Node = typename Traits::Node;
    using Index = typename Traits::Index;
    using ForwardIterator = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
    using ConstForwardIterator = typename Traits::ConstForwardIterator;
    using ConstBackwardIterator = typename Traits::ConstBackwardIterator;

    template <TreeOrder Order, Bool Reverse>
    using Traversal = TreeTraversal<typename Traits::template Iterator<const Page, Order, Reverse>, const Page*>;

private:
    Page m_root;
    Count m_size = 0;

    /** Inserta sin tomar el mutex; si la raíz se desborda, el árbol crece un nivel. */
    Bool internal_insert(const value_type& value, const ref_type& ref, Bool unique) {
        BTreeStatus status = m_root.insert(value, ref, unique);
        if (status == BTreeStatus::duplicate) return false;
        ++m_size;
        if (status == BTreeStatus::overflow) m_root.split_root();
        return true;
    }

    /** Iterador en inorden sobre la raíz de @p self: constante si el árbol lo es. */
    template <typename Self, Bool Reverse>
    using IteratorFor = typename Traits::template Iterator<std::conditional_t<std::is_const_v<Self>, const Page, Page>,
                                                           TreeOrder::InOrder, Reverse>;

    /** Rango de recorrido del árbol en el orden indicado. */
    template <TreeOrder Order, Bool Reverse> Traversal<Order, Reverse> traversal() const {
        return Traversal<Order, Reverse>(&m_root);
    }

    /** Busca sin tomar el mutex el nodo con un valor equivalente a @p value; `nullptr` si no existe. */
    const Node* find_node(const value_type& value) const { return m_root.search(value); }

    /** Capacidad de la raíz para @p order. @throws std::invalid_argument si el orden es menor que Page::min_order. */
    static Index root_capacity(Index order) {
        if (order < Page::min_order)
            throw std::invalid_argument("BTree: el orden debe ser al menos " + std::to_string(Page::min_order));
        return Page::root_capacity(order);
    }

public:
    /** Crea un árbol vacío de orden @p order. */
    explicit BTree(Index order = Traits::order) : m_root(root_capacity(order), order) {}

    /** Crea un árbol insertando los pares (valor, ref) dados. */
    BTree(std::initializer_list<std::pair<value_type, ref_type>> values) : BTree() { fill_from(*this, values); }

    /** Copia en profundidad @p other. */
    BTree(const BTree& other) : BTree(other.order()) {
        std::lock_guard lock(other.m_mutex);
        Page copy(other.m_root);
        m_root.swap(copy);
        m_size = other.m_size;
    }

    /** Mueve el contenido de @p other, que queda vacío. */
    BTree(BTree&& other) noexcept : BTree(other.order()) { swap(other); }

    /** Asigna por copia o por movimiento (copy-and-swap). */
    BTree& operator=(BTree other) noexcept {
        swap(other);
        return *this;
    }

    /** Intercambia el contenido con @p other. */
    void swap(BTree& other) noexcept {
        if (this == &other) return;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        m_root.swap(other.m_root);
        std::swap(m_size, other.m_size);
    }

    /** Inserta el valor; admite duplicados. */
    void insert(const value_type& value, const ref_type& ref) {
        std::lock_guard lock(this->m_mutex);
        internal_insert(value, ref, false);
    }

    /**
     * Inserta el valor solo si no hay uno equivalente.
     * @return `false` si ya existía.
     */
    Bool try_insert(const value_type& value, const ref_type& ref) {
        std::lock_guard lock(this->m_mutex);
        return internal_insert(value, ref, true);
    }

    /** Equivale a insert(); existe para poder leer el árbol desde un stream. */
    void push_back(const value_type& value, const ref_type& ref) { insert(value, ref); }

    /**
     * Elimina un valor equivalente a @p value.
     * @return `false` si no existía.
     */
    Bool remove(const value_type& value) {
        std::lock_guard lock(this->m_mutex);
        if (m_root.remove(value) == BTreeStatus::notfound) return false;
        --m_size;
        return true;
    }

    /** Busca un valor; devuelve una copia del guardado, que sigue siendo válida aunque otro hilo modifique el árbol. */
    [[nodiscard]] std::optional<value_type> find(const value_type& value) const {
        std::lock_guard lock(this->m_mutex);
        const Node* node = find_node(value);
        return node ? std::optional<value_type>(node->getValue()) : std::nullopt;
    }

    /** Busca la referencia asociada a @p value; devuelve una copia, o nada si no existe. */
    [[nodiscard]] std::optional<ref_type> find_ref(const value_type& value) const {
        std::lock_guard lock(this->m_mutex);
        const Node* node = find_node(value);
        return node ? std::optional<ref_type>(node->getRef()) : std::nullopt;
    }

    /** Indica si existe un valor equivalente a @p value. */
    [[nodiscard]] Bool contains(const value_type& value) const {
        std::lock_guard lock(this->m_mutex);
        return find_node(value) != nullptr;
    }

    /** Elimina todos los elementos. */
    void clear() {
        std::lock_guard lock(this->m_mutex);
        m_root.reset();
        m_size = 0;
    }

    /** Cantidad de elementos. */
    [[nodiscard]] Count size() const {
        std::lock_guard lock(this->m_mutex);
        return m_size;
    }
    /** Indica si el árbol no tiene elementos. */
    [[nodiscard]] Bool empty() const { return size() == 0; }

    /** Cantidad de niveles; 0 si está vacío. */
    [[nodiscard]] Count height() const {
        std::lock_guard lock(this->m_mutex);
        return m_size ? static_cast<Count>(m_root.height()) : 0;
    }

    /** Máximo de claves de las páginas que no son la raíz. */
    Index order() const { return m_root.order(); }

    /**
     * Iterador a la menor clave; constante si el árbol lo es. Por el iterador modificable se puede cambiar
     * la clave (`ApplyFunction` lo usa), siempre que no se altere su orden relativo.
     */
    template <typename Self> auto begin(this Self& self) { return IteratorFor<Self, false>(&self.m_root); }
    /** Iterador de fin. */
    template <typename Self> auto end(this Self&) { return IteratorFor<Self, false>(); }
    /** Iterador inverso a la mayor clave. */
    template <typename Self> auto rbegin(this Self& self) { return IteratorFor<Self, true>(&self.m_root); }
    /** Iterador inverso de fin. */
    template <typename Self> auto rend(this Self&) { return IteratorFor<Self, true>(); }

    /**
     * Iterador a la primera clave que no va antes que @p value; `end()` si no hay.
     * Como `begin()`, no toma el mutex: no usarlo mientras otro hilo modifica el árbol (para eso, `range`).
     */
    ConstForwardIterator lower_bound(const value_type& value) const {
        return ConstForwardIterator(m_root.seek(value, false));
    }
    /** Iterador a la primera clave que va después de @p value; `end()` si no hay. No toma el mutex. */
    ConstForwardIterator upper_bound(const value_type& value) const {
        return ConstForwardIterator(m_root.seek(value, true));
    }

    /**
     * Llama a @p func(nodo) con cada elemento de @p low a @p high (ambos incluidos, según el comparador), en orden,
     * con el mutex tomado.
     * @return Cantidad de elementos visitados.
     */
    template <typename Func> Count range(const value_type& low, const value_type& high, Func&& func) const {
        std::lock_guard lock(this->m_mutex);
        Count visited = 0;
        const ConstForwardIterator last = upper_bound(high);
        for (auto it = lower_bound(low); it != last && it != end(); ++it, ++visited)
            func(*it);
        return visited;
    }

    /** Vista del árbol para visualization.h: cada página es un nodo con hasta `orden + 1` hijos. */
    struct View {
        using Handle = typename Page::ConstPagePtr;
        /** Texto que separa las claves en la etiqueta de una página. */
        static constexpr std::string_view key_separator = "|";
        const BTree* tree;

        /** Página raíz. */
        Handle root() const { return &tree->m_root; }
        /** Subpáginas de @p page de izquierda a derecha; en las hojas son todas nulas. */
        typename Traits::template Storage<Handle> children(Handle page) const {
            typename Traits::template Storage<Handle> kids;
            for (Index i = 0; i <= page->size(); ++i)
                kids.push_back(page->child(i, false));
            return kids;
        }
        /** Indica si la página existe y tiene claves. */
        Bool valid(Handle page) const { return page && page->size() > 0; }
        /** Escribe las claves de la página separadas por `key_separator`. */
        void label(std::ostream& os, Handle page) const {
            for (Index i = 0; i < page->size(); ++i)
                os << (i ? key_separator : "") << page->key(i, false).getValue();
        }
    };

    /** Devuelve la vista de árbol que usa `print_tree`. */
    View tree_view() const { return View{this}; }
};

#endif // CONTAINERS_BTREE_H
