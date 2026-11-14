#ifndef CONTAINERS_BINARYTREEITERATOR_H
#define CONTAINERS_BINARYTREEITERATOR_H

#include "GeneralIterator.h"
#include <memory>

/** Órdenes de recorrido de un árbol binario. */
enum class TreeOrder { PreOrder, InOrder, PostOrder };

/** Baja desde @p cur siempre por el lado @p side y devuelve el último nodo: el mínimo del subárbol (@p side falso) o el máximo (@p side verdadero). */
template <typename Node>
Node* extreme(Node* cur, bool side) {
    while (cur && cur->m_pChild[side])
        cur = cur->m_pChild[side];
    return cur;
}

/** Indica de qué lado de su padre cuelga @p node: `true` si es el hijo derecho. */
template <typename Node>
bool side_of(const Node* node) {
    return node->m_pParent && node->m_pParent->m_pChild[1] == node;
}

/** Libera recursivamente el subárbol con raíz en @p node. */
template <typename Node>
void destroy(Node* node) {
    if (!node) return;
    destroy(node->m_pChild[0]);
    destroy(node->m_pChild[1]);
    delete node;
}

/** Copia recursivamente el subárbol @p source y lo cuelga de @p parent. Si la copia falla a medias, libera lo ya copiado. */
template <typename Node>
Node* clone(const Node* source, Node* parent = nullptr) {
    if (!source) return nullptr;
    std::unique_ptr<Node, void (*)(Node*)> copy(new Node(*source), &destroy<Node>);
    copy->m_pParent = parent;
    copy->m_pChild[0] = copy->m_pChild[1] = nullptr;
    copy->m_pChild[0] = clone(source->m_pChild[0], copy.get());
    copy->m_pChild[1] = clone(source->m_pChild[1], copy.get());
    return copy.release();
}

/**
 * Base de las políticas de recorrido: define qué hijo se visita primero.
 * @tparam Reverse Si es `true`, se intercambian izquierda y derecha.
 */
template <bool Reverse>
struct TraversalBase {
    static constexpr bool L = Reverse;
    static constexpr bool R = !Reverse;

    /** Devuelve el primer hijo existente de @p cur en el sentido del recorrido. */
    template <typename Node>
    static Node* firstChild(Node* cur) {
        return cur->m_pChild[L] ? cur->m_pChild[L] : cur->m_pChild[R];
    }

    /** Baja desde @p cur por el primer hijo disponible hasta llegar a una hoja. */
    template <typename Node>
    static Node* deepest(Node* cur) {
        while (cur && (cur->m_pChild[L] || cur->m_pChild[R]))
            cur = firstChild(cur);
        return cur;
    }
};

/**
 * Política de recorrido: `first()` da el nodo inicial y `next()` el sucesor, sin pila ni recursión, usando el puntero al padre.
 * @tparam Order Orden del recorrido.
 * @tparam Reverse Si es `true`, recorre en espejo.
 */
template <TreeOrder Order, bool Reverse>
struct TraversalPolicy;

/** Recorrido en inorden: izquierda, nodo, derecha. */
template <bool Reverse>
struct TraversalPolicy<TreeOrder::InOrder, Reverse> : TraversalBase<Reverse> {
    using Base = TraversalBase<Reverse>;
    using Base::L;
    using Base::R;

    /** Primer nodo: el extremo izquierdo del árbol. */
    template <typename Node>
    static Node* first(Node* root) { return extreme(root, L); }

    /** Sucesor en inorden de @p cur, o `nullptr` si era el último. */
    template <typename Node>
    static Node* next(Node* cur) {
        if (cur->m_pChild[R])
            return extreme(cur->m_pChild[R], L);
        Node* p = cur->m_pParent;
        while (p && cur == p->m_pChild[R]) {
            cur = p;
            p = p->m_pParent;
        }
        return p;
    }
};

/** Recorrido en preorden: nodo, izquierda, derecha. */
template <bool Reverse>
struct TraversalPolicy<TreeOrder::PreOrder, Reverse> : TraversalBase<Reverse> {
    using Base = TraversalBase<Reverse>;
    using Base::L;
    using Base::R;

    /** Primer nodo: la raíz. */
    template <typename Node>
    static Node* first(Node* root) { return root; }

    /** Sucesor en preorden de @p cur, o `nullptr` si era el último. */
    template <typename Node>
    static Node* next(Node* cur) {
        if (cur->m_pChild[L] || cur->m_pChild[R])
            return Base::firstChild(cur);
        Node* p = cur->m_pParent;
        while (p && (cur == p->m_pChild[R] || !p->m_pChild[R])) {
            cur = p;
            p = p->m_pParent;
        }
        return p ? p->m_pChild[R] : nullptr;
    }
};

/** Recorrido en postorden: izquierda, derecha, nodo. */
template <bool Reverse>
struct TraversalPolicy<TreeOrder::PostOrder, Reverse> : TraversalBase<Reverse> {
    using Base = TraversalBase<Reverse>;
    using Base::L;
    using Base::R;

    /** Primer nodo: la hoja a la que se llega bajando siempre por el primer hijo disponible. */
    template <typename Node>
    static Node* first(Node* root) { return Base::deepest(root); }

    /** Sucesor en postorden de @p cur, o `nullptr` si era el último. */
    template <typename Node>
    static Node* next(Node* cur) {
        Node* p = cur->m_pParent;
        if (p && cur == p->m_pChild[L] && p->m_pChild[R])
            return Base::deepest(p->m_pChild[R]);
        return p;
    }
};

/**
 * Iterador de árbol binario que avanza con la política TraversalPolicy.
 * @tparam Node Tipo de nodo.
 * @tparam Order Orden del recorrido.
 * @tparam Reverse Si es `true`, recorre en espejo.
 */
template <typename Node, TreeOrder Order = TreeOrder::InOrder, bool Reverse = false>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<Node, Order, Reverse>, Node> {
    using Policy = TraversalPolicy<Order, Reverse>;

public:
    using value_type        = Node;
    using MySelf            = BinaryTreeIterator<Node, Order, Reverse>;
    using Parent            = GeneralIterator<MySelf, Node>;

    /** Crea el iterador en el primer nodo del recorrido del árbol @p root; con `nullptr` es el iterador de fin. */
    BinaryTreeIterator(Node* root = nullptr) : Parent(Policy::first(root)) {}

    /** Crea un iterador situado directamente en @p node. */
    static BinaryTreeIterator at(Node* node) {
        BinaryTreeIterator it;
        it.m_ptr = node;
        return it;
    }

    // Prefix increment
    /** Avanza al siguiente nodo del recorrido. */
    BinaryTreeIterator& operator++() {
        Parent::m_ptr = Policy::next(Parent::m_ptr);
        return *this;
    }
};

/**
 * Rango `begin()`/`end()` sobre un árbol, para usarlo en un `for` por rango.
 * @tparam Iterator Iterador que fija el orden del recorrido.
 */
template <typename Iterator>
class TreeTraversal {
    typename Iterator::pointer m_root;

public:
    /** Crea el rango sobre el árbol con raíz @p root. */
    explicit TreeTraversal(typename Iterator::pointer root) : m_root(root) {}
    /** Iterador al primer nodo del recorrido. */
    Iterator begin() const { return Iterator(m_root); }
    /** Iterador de fin. */
    Iterator end() const { return Iterator(nullptr); }
};

#endif // CONTAINERS_BINARYTREEITERATOR_H
