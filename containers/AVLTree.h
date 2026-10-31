#ifndef __AVLTREE_H__
#define __AVLTREE_H__
#include "binarytree.h"

// Nodo AVL
template <typename T>
class AVLNode : public BaseBinaryNode<T, AVLNode<T>> {
public:
    using Parent = BaseBinaryNode<T, AVLNode<T>>;
    using Parent::Parent;
    TX m_height = 1;
};

template <typename T>
struct AVLTreeTraits {
    using Node              = AVLNode<T>;
    using ForwardIterator   = BinaryTreeIterator<Node, InOrder, Asc>;
    using BackwardIterator  = BinaryTreeIterator<Node, InOrder, Desc>;
};

template <typename T>
struct AVLTreeAscTraits : public AscendingTraits<T>, public AVLTreeTraits<T> {};

template <typename T>
struct AVLTreeDescTraits : public DescendingTraits<T>, public AVLTreeTraits<T> {};

template <typename Traits>
class AVLTree : public BinaryTree<Traits> {
public:
    using Parent = BinaryTree<Traits>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using typename Parent::Compare;

protected:
    using Parent::m_root;
    using Parent::m_mutex;
    using Parent::internal_remove;

private:
    static TX Height(NodePtr node) { return node ? node->m_height : 0; }
    static void UpdateHeight(NodePtr node) {
        node->m_height = 1 + std::max(Height(node->m_pChild[0]), Height(node->m_pChild[1]));
    }

    NodePtr Rotate(NodePtr x, TX dir) {
        NodePtr y = x->m_pChild[1-dir];
        x->m_pChild[1-dir] = y->m_pChild[dir];
        if (y->m_pChild[dir])
            y->m_pChild[dir]->m_parent = x;
        y->m_pChild[dir] = x;
        y->m_parent = x->m_parent;
        x->m_parent = y;
        UpdateHeight(x);
        UpdateHeight(y);
        return y;
    }

    // sube hasta la raiz
    void Redistribution(NodePtr from, bool cut_on_first_rotation) {
        for (NodePtr n = from; n; ) {
            UpdateHeight(n);
            TX balance = Height(n->m_pChild[1]) - Height(n->m_pChild[0]);
            if (balance > -2 && balance < 2) {
                n = n->m_parent;
                continue;
            }
            NodePtr old = n;
            NodePtr child = n->m_pChild[balance > 0 ? 1 : 0];
            TX cbalance = Height(child->m_pChild[1]) - Height(child->m_pChild[0]);
            if (balance > 0) {
                if (cbalance < 0)
                    n->m_pChild[1] = Rotate(n->m_pChild[1], 1);
                n = Rotate(n, 0);
            } else {
                if (cbalance > 0)
                    n->m_pChild[0] = Rotate(n->m_pChild[0], 0);
                n = Rotate(n, 1);
            }
            NodePtr p = n->m_parent;
            if (p)
                p->m_pChild[p->m_pChild[0] == old ? 0 : 1] = n;
            else
                m_root = n;
            if (cut_on_first_rotation)
                return;
            n = p;
        }
    }

public:
    void insert(const value_type& value, Ref ref) override {
        std::lock_guard lock(this->m_mutex);
        NodePtr leaf = nullptr;
        BinaryTreeInsert<Node, Compare>(m_root, value, ref, nullptr, &leaf);
        Redistribution(leaf, true);
    }

    Node remove(const value_type& value) override {
        std::lock_guard lock(this->m_mutex);
        NodePtr removed = nullptr, lowest = nullptr;
        internal_remove(m_root, nullptr, value, &removed, &lowest);
        if (!removed)
            return Node();
        Node result(removed->getValue(), removed->getRef());
        delete removed;
        Redistribution(lowest, false);
        return result;
    }

    void PrintNode(std::ostream &os, NodePtr node, Color color) const override {
        Parent::PrintNode(os, node, color);
        os << " h=" << node->m_height;
    }
};

#endif // __AVLTREE_H__
