#ifndef __AVL_H__
#define __AVL_H__

#include "binarytree.h"
#include <algorithm>
#include <cstdlib>

template <typename T> class AVLNode : public BinaryTreeNode<T, AVLNode<T>> {
public:
    using Parent = BinaryTreeNode<T, AVLNode<T>>;
    using Parent::Parent;

    int m_balance = 0;
};

template <typename Traits, typename Ops = SearchTraversalOps>
class AVLTree : public BinaryTree<Traits, Ops> {
public:
    using Parent = BinaryTree<Traits, Ops>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using Parent::height;

    AVLTree() = default;

    AVLTree(std::initializer_list<std::pair<value_type, Ref>> values) { fill_from(*this, values); }

protected:
    using Parent::side_of;

    void after_insert(NodePtr node) override {
        for (; node->m_pParent; node = node->m_pParent) {
            NodePtr parent = node->m_pParent;
            parent->m_balance += side_of(node) ? 1 : -1;
            if (parent->m_balance == 0) return;
            if (std::abs(parent->m_balance) == 2) {
                redistribute(parent);
                return;
            }
        }
    }

    void after_remove(NodePtr parent, bool side) override {
        for (; parent; side = side_of(parent), parent = parent->m_pParent) {
            parent->m_balance -= side ? 1 : -1;
            if (std::abs(parent->m_balance) == 1) return;
            if (std::abs(parent->m_balance) == 2) {
                parent = redistribute(parent);
                if (parent->m_balance != 0) return;
            }
        }
    }

    NodePtr replacement(NodePtr node) const override {
        bool side = node->m_balance >= 0;
        return extreme(node->m_pChild[side], !side);
    }

    void take_place(NodePtr repl, const Node* node) override { repl->m_balance = node->m_balance; }

    std::size_t height(const Node* node) const override {
        std::size_t h = 0;
        for (; node; ++h)
            node = node->m_pChild[node->m_balance > 0];
        return h;
    }

    void print_node(std::ostream& os, const Node& node) const override {
        os << node << '[' << node.m_balance << ']';
    }

private:
    NodePtr rotate(NodePtr x, bool dir) {
        NodePtr y = Parent::rotate(x, dir);
        int s = dir ? -1 : 1;
        int bx = s * x->m_balance;
        int by = s * y->m_balance;
        bx = bx - 1 - std::max(by, 0);
        by = by - 1 + std::min(bx, 0);
        x->m_balance = s * bx;
        y->m_balance = s * by;
        return y;
    }

    NodePtr redistribute(NodePtr x) {
        bool heavy = x->m_balance > 0;
        NodePtr child = x->m_pChild[heavy];
        if (child->m_balance * x->m_balance < 0)
            rotate(child, heavy);
        return rotate(x, !heavy);
    }
};

#endif // __AVL_H__
