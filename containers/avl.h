#ifndef __AVL_H__
#define __AVL_H__
#include <algorithm>
#include "binarytree.h"

template <typename T>
class AVLNode : public BinaryTreeNode<T, AVLNode<T>> {
public:
    using Parent = BinaryTreeNode<T, AVLNode<T>>;
    using Parent::Parent;

    TX m_height = 1;
};

template <typename T>
struct AVLAscTraits : public AscendingTraits<T>, public BinaryTreeTraits<T, AVLNode<T>> {};

template <typename T>
struct AVLDescTraits : public DescendingTraits<T>, public BinaryTreeTraits<T, AVLNode<T>> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseAVL;

template <typename Traits>
using AVL = BaseAVL<Traits, Callable, Applicable, Searchable, ReverseCallable, Printable, Readable>;

template <typename Traits, template <typename> typename... Mixins>
class BaseAVL : public BaseBinaryTree<Traits, Mixins...> {
public:
    using Parent = BaseBinaryTree<Traits, Mixins...>;
    using typename Parent::NodePtr;

protected:
    void rebalance(NodePtr node) override;

private:
    static TX height(NodePtr node) { return node ? node->m_height : 0; }
    static void update_height(NodePtr node) { node->m_height = 1 + std::max(height(node->m_pChild[0]), height(node->m_pChild[1])); }
    NodePtr rotate(NodePtr node, TX side);
};

template <typename Traits, template <typename> typename... Mixins>
void BaseAVL<Traits, Mixins...>::rebalance(NodePtr node) {
    for (; node; node = node->m_parent) {
        update_height(node);
        TX side = height(node->m_pChild[1]) > height(node->m_pChild[0]);
        NodePtr child = node->m_pChild[side];
        if (height(child) - height(node->m_pChild[1-side]) > 1) {
            if (height(child->m_pChild[1-side]) > height(child->m_pChild[side]))
                rotate(child, 1-side);
            node = rotate(node, side);
        }
    }
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseAVL<Traits, Mixins...>::rotate(NodePtr node, TX side) -> NodePtr {
    NodePtr child = node->m_pChild[side], inner = child->m_pChild[1-side];
    this->transplant(node, child);
    node->m_pChild[side] = inner;
    if (inner)
        inner->m_parent = node;
    child->m_pChild[1-side] = node;
    node->m_parent = child;
    update_height(node);
    update_height(child);
    return child;
}

#endif // __AVL_H__
