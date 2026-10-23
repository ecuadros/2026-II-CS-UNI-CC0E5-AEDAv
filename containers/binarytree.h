#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include <initializer_list>
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "traits.h"

template <typename T>
class BinaryTreeNode : public GeneralNode<T> {
public:
    using value_type        = T;
    using MySelf            = BinaryTreeNode<T>;
    using Node              = MySelf;
    using NodePtr           = Node*;
    using Parent            = GeneralNode<MySelf>;
    using Parent::Parent; // Inherit constructor

    NodePtr m_pChild[2] = {nullptr, nullptr}; // left and right children

};

template <typename T>
class BinaryTreeForwardIterator : public GeneralIterator<BinaryTreeForwardIterator<T>, BinaryTreeNode<T>> {
public:
    using value_type = BinaryTreeNode<T>;
    using MySelf = BinaryTreeForwardIterator<T>;
    using Parent = GeneralIterator<MySelf, value_type>;
    using Parent::Parent; // Inherit constructor
    BinaryTreeForwardIterator& operator++() { Parent::m_ptr = Parent::m_ptr->m_pChild[1]; return *this; }
};

template <typename T>
class BinaryTreeBackwardIterator : public GeneralIterator<BinaryTreeBackwardIterator<T>, BinaryTreeNode<T>> {
public:
    using value_type = BinaryTreeNode<T>;
    using MySelf = BinaryTreeBackwardIterator<T>;
    using Parent = GeneralIterator<MySelf, value_type>;
    using Parent::Parent; // Inherit constructor
    BinaryTreeBackwardIterator& operator++() { Parent::m_ptr = Parent::m_ptr->m_pPrev; return *this; }
};


template <typename T>
struct BinaryTreeAscTraits : public AscendingTraits<T> {
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeForwardIterator<T>;  // itera sobre Node, no sobre T
    using BackwardIterator   = BinaryTreeBackwardIterator<T>;
};

template <typename T>
struct BinaryTreeDescTraits : public DescendingTraits<T> {
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeForwardIterator<T>;  // itera sobre Node, no sobre T
    using BackwardIterator   = BinaryTreeBackwardIterator<T>;
};

template <typename Traits>
class BinaryTree {
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node *;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using BackwardIterator  = typename Traits::BackwardIterator;

private:
    NodePtr m_root = nullptr; // root of the binary tree
    void internal_insert(NodePtr& node, const value_type& value, Ref ref) {
        if (!node) {
            node = new Node(value, ref);
            return;
        }
        auto branch = value > node->m_value;
        internal_insert(node->m_pChild[branch], value, ref); // Tail recursion to insert in the correct branch
    }
public:
    BinaryTree() {}
    BinaryTree(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            insert(v.first, v.second);
    }
    void insert(const value_type& value, Ref ref) {
        // Implement insertion logic for binary tree
        internal_insert(m_root, value, ref);
    }
    // Move constructor and move assignment operator
    // Constructor and destructor
    // Persistencia (operator << y operator >> globales para todas las estructuras pero si es para BT con indentacion entonces hacerlo por separado)
    // Concurrencia
    // Iteradores (forward y backward)
    // FirstThat, ForEach, call, rcall por herencia
};
#endif // __BINARYTREE_H__
