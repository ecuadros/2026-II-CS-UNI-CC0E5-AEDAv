#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__

#include "GeneralNode.h"
#include "traits.h"
#include "traversalops.h"
#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <mutex>
#include <ostream>
#include <type_traits>
#include <utility>

/**
 * Nodo de árbol binario con punteros a sus dos hijos y a su padre.
 * @tparam T Tipo del valor.
 * @tparam Derived Nodo derivado al que apuntan los enlaces, o `void` si es este mismo.
 * @tparam R Tipo de la referencia.
 */
template <typename T, typename Derived, typename R>
class BinaryTreeNode : public GeneralNode<T, R> {
public:
    using Parent = GeneralNode<T, R>;
    using Parent::Parent;
    using Node = NodeSelf<BinaryTreeNode<T, void, R>, Derived>;
    using NodePtr = Node*;

    /** Hijos: `[0]` izquierdo y `[1]` derecho. */
    NodePtr m_pChild[2] = {nullptr, nullptr};
    /** Padre, o `nullptr` en la raíz. */
    NodePtr m_pParent = nullptr;

    /** Indica si el nodo no tiene hijos. */
    bool is_leaf() const { return !m_pChild[0] && !m_pChild[1]; }
};

/**
 * Base común de los árboles binarios: raíz, copia, destrucción, altura, iteradores y recorridos. No define cómo se insertan los nodos.
 * @tparam Traits Tipos de nodo e iteradores.
 * @tparam Ops Mixins de recorrido, impresión y lectura.
 */
template <typename Traits, typename Ops> class BinaryTreeBase : public Ops {
public:
    using Node = typename Traits::Node;
    using NodePtr = Node*;
    using ForwardIterator = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;

    template <TreeOrder Order, bool Reverse>
    using Traversal = TreeTraversal<BinaryTreeIterator<const Node, Order, Reverse>>;

protected:
    /** Raíz del árbol, o `nullptr` si está vacío. */
    NodePtr m_root = nullptr;

    /** Libera todos los nodos sin tomar el mutex; las clases derivadas lo amplían para limpiar su propio estado. */
    virtual void reset() { destroy(std::exchange(m_root, nullptr)); }
    /** Escribe el texto con el que se dibuja un nodo. */
    virtual void print_node(std::ostream& os, const Node& node) const { os << node; }

    /** Altura del subárbol con raíz en @p node; 0 si es nulo. */
    virtual std::size_t height(const Node* node) const {
        if (!node) return 0;
        return 1 + std::max(height(node->m_pChild[0]), height(node->m_pChild[1]));
    }

    /** Devuelve el rango de recorrido del árbol en el orden indicado. */
    template <TreeOrder Order, bool Reverse> Traversal<Order, Reverse> traversal() const {
        return Traversal<Order, Reverse>(m_root);
    }

public:
    /** Crea un árbol vacío. */
    BinaryTreeBase() = default;

    /** Copia en profundidad los nodos de @p other. */
    BinaryTreeBase(const BinaryTreeBase& other) {
        std::lock_guard lock(other.m_mutex);
        m_root = clone(other.m_root);
    }

    /** La asignación la definen las clases derivadas. */
    BinaryTreeBase& operator=(const BinaryTreeBase&) = delete;

    /** Libera todos los nodos. */
    virtual ~BinaryTreeBase() { clear(); }

    /** Cantidad de elementos. */
    virtual std::size_t size() const = 0;
    /** Indica si el árbol no tiene elementos. */
    bool empty() const { return size() == 0; }

    /** Elimina todos los nodos. */
    void clear() {
        std::lock_guard lock(this->m_mutex);
        reset();
    }

    /** Altura del árbol; 0 si está vacío. */
    std::size_t height() const {
        std::lock_guard lock(this->m_mutex);
        return height(m_root);
    }

    /** Vista del árbol para visualization.h; los nodos se identifican por su puntero. */
    struct View {
        using Handle = const Node*;
        const BinaryTreeBase* tree;

        /** Raíz del árbol. */
        Handle root() const { return tree->m_root; }
        /** Hijo izquierdo (@p side falso) o derecho (@p side verdadero) de @p node. */
        Handle child(Handle node, bool side) const { return node->m_pChild[side]; }
        /** Indica si el nodo existe. */
        bool valid(Handle node) const { return node != nullptr; }
        /** Escribe el texto del nodo. */
        void label(std::ostream& os, Handle node) const { tree->print_node(os, *node); }
        /** Altura del subárbol de @p node. */
        std::size_t height(Handle node) const { return tree->height(node); }
    };

    /** Devuelve la vista de árbol que usa `print_tree`. */
    View tree_view() const { return View{this}; }

    /** Iterador al primer nodo en el orden por defecto del árbol. */
    ForwardIterator begin() const { return ForwardIterator(m_root); }
    /** Iterador de fin. */
    ForwardIterator end() const { return ForwardIterator(nullptr); }
    /** Iterador inverso al último nodo. */
    BackwardIterator rbegin() const { return BackwardIterator(m_root); }
    /** Iterador inverso de fin. */
    BackwardIterator rend() const { return BackwardIterator(nullptr); }

    /** Recorrido en inorden. */
    auto InOrder() const { return traversal<TreeOrder::InOrder, false>(); }
    /** Recorrido en inorden invertido. */
    auto ReverseInOrder() const { return traversal<TreeOrder::InOrder, true>(); }
    /** Recorrido en preorden. */
    auto PreOrder() const { return traversal<TreeOrder::PreOrder, false>(); }
    /** Recorrido en preorden invertido; se obtiene como el postorden en espejo. */
    auto ReversePreOrder() const { return traversal<TreeOrder::PostOrder, true>(); }
    /** Recorrido en postorden. */
    auto PostOrder() const { return traversal<TreeOrder::PostOrder, false>(); }
    /** Recorrido en postorden invertido; se obtiene como el preorden en espejo. */
    auto ReversePostOrder() const { return traversal<TreeOrder::PreOrder, true>(); }
};

/**
 * Árbol binario de búsqueda sin balanceo. Sirve de base al AVL mediante los ganchos `after_insert`, `after_remove`, `replacement` y `take_place`.
 * @tparam Traits Tipos de valor, nodo, comparador e iteradores.
 * @tparam Ops Mixins de recorrido, impresión y lectura.
 */
template <typename Traits, typename Ops = SearchTraversalOps>
class BinaryTree : public BinaryTreeBase<Traits, Ops> {
public:
    using Base = BinaryTreeBase<Traits, Ops>;
    using value_type = typename Traits::value_type;
    using Node = typename Traits::Node;
    using ref_type = typename Traits::ref_type;
    using Compare = typename Traits::Compare;
    using NodePtr = Node*;

private:
    std::size_t m_size = 0;
    Compare m_comp;

    /** Indica si dos valores son equivalentes para el comparador: ninguno va antes que el otro. */
    bool equivalent(const value_type& a, const value_type& b) const {
        return !m_comp(a, b) && !m_comp(b, a);
    }

    /**
     * Inserta el valor sin tomar el mutex. Si @p unique es `true` y ya hay un valor equivalente, no inserta.
     * @return El nodo insertado o el ya existente, y si hubo inserción.
     */
    std::pair<NodePtr, bool> internal_insert(const value_type& value, const ref_type& ref, bool unique) {
        NodePtr* slot = &this->m_root;
        NodePtr parent = nullptr;
        while (*slot) {
            parent = *slot;
            if (unique && equivalent(value, parent->getValue())) return {parent, false};
            slot = &parent->m_pChild[m_comp(parent->getValue(), value)];
        }
        NodePtr node = new Node(value, ref);
        node->m_pParent = parent;
        *slot = node;
        ++m_size;
        after_insert(node);
        return {node, true};
    }

    /** Busca el nodo con un valor equivalente a @p value; `nullptr` si no existe. */
    NodePtr find_node(const value_type& value) const {
        NodePtr node = this->m_root;
        while (node && !equivalent(value, node->getValue()))
            node = node->m_pChild[m_comp(node->getValue(), value)];
        return node;
    }

protected:
    /** Gancho que se llama tras enlazar un nodo nuevo. */
    virtual void after_insert(NodePtr) {}
    /** Gancho que se llama tras quitar un nodo, con el padre y el lado donde el subárbol perdió altura. */
    virtual void after_remove(NodePtr, bool) {}
    /** Elige el nodo que ocupará el lugar de @p node cuando este tiene dos hijos; por defecto, su sucesor en inorden. */
    virtual NodePtr replacement(NodePtr node) const { return extreme(node->m_pChild[1], 0); }

    /** Libera los nodos y pone el tamaño a cero. */
    void reset() override {
        Base::reset();
        m_size = 0;
    }

    /** Devuelve el puntero que apunta a @p node: el de su padre o la raíz. */
    NodePtr& link_of(NodePtr node) {
        return node->m_pParent ? node->m_pParent->m_pChild[side_of(node)] : this->m_root;
    }

    /** Gancho que se llama cuando un nodo ocupa la posición de otro que se elimina. */
    virtual void take_place(NodePtr, const Node*) {}

    /** Desengancha un nodo que tiene como máximo un hijo, subiendo ese hijo a su lugar. */
    void detach(NodePtr node) {
        NodePtr child = node->m_pChild[0] ? node->m_pChild[0] : node->m_pChild[1];
        link_of(node) = child;
        if (child) child->m_pParent = node->m_pParent;
    }

    /**
     * Saca @p node del árbol sin liberarlo.
     * @return El padre y el lado desde donde se quitó físicamente un nodo, para rebalancear.
     */
    std::pair<NodePtr, bool> unlink(NodePtr node) {
        if (!node->m_pChild[0] || !node->m_pChild[1]) {
            std::pair<NodePtr, bool> origin{node->m_pParent, side_of(node)};
            detach(node);
            return origin;
        }
        NodePtr repl = replacement(node);
        std::pair<NodePtr, bool> origin{repl->m_pParent == node ? repl : repl->m_pParent, side_of(repl)};
        detach(repl);
        for (bool s : {false, true}) {
            repl->m_pChild[s] = node->m_pChild[s];
            if (repl->m_pChild[s]) repl->m_pChild[s]->m_pParent = repl;
        }
        link_of(node) = repl;
        repl->m_pParent = node->m_pParent;
        take_place(repl, node);
        return origin;
    }

    /**
     * Rota el subárbol de @p x hacia el lado @p dir (`true` es a la derecha).
     * @return El nodo que queda como nueva raíz del subárbol.
     */
    NodePtr rotate(NodePtr x, bool dir) {
        NodePtr y = x->m_pChild[!dir];
        x->m_pChild[!dir] = y->m_pChild[dir];
        if (y->m_pChild[dir]) y->m_pChild[dir]->m_pParent = x;
        link_of(x) = y;
        y->m_pParent = x->m_pParent;
        y->m_pChild[dir] = x;
        x->m_pParent = y;
        return y;
    }

public:
    /** Crea un árbol vacío. */
    BinaryTree() = default;

    /** Crea un árbol insertando los pares (valor, ref) dados. */
    BinaryTree(std::initializer_list<std::pair<value_type, ref_type>> values) { fill_from(*this, values); }

    /** Copia en profundidad @p other. */
    BinaryTree(const BinaryTree& other) : Base(other) {
        std::lock_guard lock(other.m_mutex);
        m_size = other.m_size;
    }

    /** Mueve el contenido de @p other, que queda vacío. */
    BinaryTree(BinaryTree&& other) noexcept { swap(other); }

    /** Asigna por copia o por movimiento (copy-and-swap). */
    BinaryTree& operator=(BinaryTree other) noexcept {
        swap(other);
        return *this;
    }

    /** Intercambia el contenido con @p other. */
    void swap(BinaryTree& other) noexcept {
        if (this == &other) return;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        std::swap(this->m_root, other.m_root);
        std::swap(m_size, other.m_size);
    }

    /** Inserta el valor; admite duplicados. */
    void insert(const value_type& value, const ref_type& ref) {
        std::lock_guard lock(this->m_mutex);
        internal_insert(value, ref, false);
    }

    /**
     * Elimina un nodo con valor equivalente a @p value.
     * @return `false` si no existía.
     */
    bool remove(const value_type& value) {
        std::lock_guard lock(this->m_mutex);
        NodePtr node = find_node(value);
        if (!node) return false;
        auto [parent, side] = unlink(node);
        delete node;
        --m_size;
        after_remove(parent, side);
        return true;
    }

    /**
     * Busca un valor.
     * @return Puntero al valor guardado, o `nullptr` si no existe.
     */
    const value_type* find(const value_type& value) const {
        std::lock_guard lock(this->m_mutex);
        NodePtr node = find_node(value);
        return node ? &node->getValue() : nullptr;
    }

    /** Devuelve la referencia asociada a @p value, insertándolo con una referencia por defecto si no existía. */
    ref_type& operator[](const value_type& value) {
        std::lock_guard lock(this->m_mutex);
        return internal_insert(value, ref_type{}, true).first->ref();
    }

    /**
     * Busca la referencia asociada a @p value.
     * @return Puntero a la referencia (constante si el árbol lo es), o `nullptr` si no existe.
     */
    template <typename Self> auto find_ref(this Self& self, const value_type& value) {
        using Ptr = std::conditional_t<std::is_const_v<Self>, const ref_type*, ref_type*>;
        std::lock_guard lock(self.m_mutex);
        NodePtr node = self.find_node(value);
        return node ? Ptr(&node->ref()) : nullptr;
    }

    /** Devuelve un iterador situado en el nodo de @p value, o `end()` si no existe. */
    typename Base::ForwardIterator locate(const value_type& value) const {
        std::lock_guard lock(this->m_mutex);
        return Base::ForwardIterator::at(find_node(value));
    }

    /**
     * Inserta el valor solo si no hay uno equivalente.
     * @return Iterador al nodo insertado o existente, y si hubo inserción.
     */
    std::pair<typename Base::ForwardIterator, bool> try_insert(const value_type& value, const ref_type& ref) {
        std::lock_guard lock(this->m_mutex);
        auto [node, inserted] = internal_insert(value, ref, true);
        return {Base::ForwardIterator::at(node), inserted};
    }

    /** Indica si existe un valor equivalente a @p value. */
    bool contains(const value_type& value) const {
        std::lock_guard lock(this->m_mutex);
        return find_node(value) != nullptr;
    }

    /** Equivale a insert(); existe para poder leer el árbol desde un stream. */
    void push_back(const value_type& value, const ref_type& ref) { insert(value, ref); }

    /** Cantidad de elementos. */
    std::size_t size() const override {
        std::lock_guard lock(this->m_mutex);
        return m_size;
    }
};

#endif // __BINARYTREE_H__
