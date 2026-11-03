#ifndef CONTAINERS_BINARYTREEITERATOR_H
#define CONTAINERS_BINARYTREEITERATOR_H

#include "GeneralIterator.h"

enum class TreeOrder { PreOrder, InOrder, PostOrder };

template <typename Node>
Node* extreme(Node* cur, bool side) {
    while (cur && cur->m_pChild[side])
        cur = cur->m_pChild[side];
    return cur;
}

template <bool Reverse>
struct TraversalBase {
    static constexpr bool L = Reverse;
    static constexpr bool R = !Reverse;

    template <typename Node>
    static Node* firstChild(Node* cur) {
        return cur->m_pChild[L] ? cur->m_pChild[L] : cur->m_pChild[R];
    }

    template <typename Node>
    static Node* deepest(Node* cur) {
        while (cur && (cur->m_pChild[L] || cur->m_pChild[R]))
            cur = firstChild(cur);
        return cur;
    }
};

template <TreeOrder Order, bool Reverse>
struct TraversalPolicy;

template <bool Reverse>
struct TraversalPolicy<TreeOrder::InOrder, Reverse> : TraversalBase<Reverse> {
    using Base = TraversalBase<Reverse>;
    using Base::L;
    using Base::R;

    template <typename Node>
    static Node* first(Node* root) { return extreme(root, L); }

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

template <bool Reverse>
struct TraversalPolicy<TreeOrder::PreOrder, Reverse> : TraversalBase<Reverse> {
    using Base = TraversalBase<Reverse>;
    using Base::L;
    using Base::R;

    template <typename Node>
    static Node* first(Node* root) { return root; }

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

template <bool Reverse>
struct TraversalPolicy<TreeOrder::PostOrder, Reverse> : TraversalBase<Reverse> {
    using Base = TraversalBase<Reverse>;
    using Base::L;
    using Base::R;

    template <typename Node>
    static Node* first(Node* root) { return Base::deepest(root); }

    template <typename Node>
    static Node* next(Node* cur) {
        Node* p = cur->m_pParent;
        if (p && cur == p->m_pChild[L] && p->m_pChild[R])
            return Base::deepest(p->m_pChild[R]);
        return p;
    }
};

template <typename Node, TreeOrder Order = TreeOrder::InOrder, bool Reverse = false>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<Node, Order, Reverse>, Node> {
    using Policy = TraversalPolicy<Order, Reverse>;

public:
    using value_type        = Node;
    using MySelf            = BinaryTreeIterator<Node, Order, Reverse>;
    using Parent            = GeneralIterator<MySelf, Node>;

    BinaryTreeIterator(Node* root = nullptr) : Parent(Policy::first(root)) {}

    // Prefix increment
    BinaryTreeIterator& operator++() {
        Parent::m_ptr = Policy::next(Parent::m_ptr);
        return *this;
    }
};

template <typename Iterator>
class TreeTraversal {
    typename Iterator::pointer m_root;

public:
    explicit TreeTraversal(typename Iterator::pointer root) : m_root(root) {}
    Iterator begin() const { return Iterator(m_root); }
    Iterator end() const { return Iterator(nullptr); }
};

#endif // CONTAINERS_BINARYTREEITERATOR_H
