#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include <utility>
#include "GeneralContainer.h"

template <typename T>
class BinaryTreeNode : public GeneralNode<T> {
public:
    using value_type        = T;
    using MySelf            = BinaryTreeNode<T>;
    using Node              = MySelf;
    using NodePtr           = Node*;
    //using Parent            = GeneralNode<MySelf>;
    using Parent = GeneralNode<T>;
    using Parent::Parent; // Inherit constructor

    NodePtr m_pChild[2] = {nullptr, nullptr}; // left and right children

    NodePtr m_parent = nullptr;

};
//
template <typename T>
class BinaryTreeForwardIterator
    : public GeneralIterator<BinaryTreeForwardIterator<T>, BinaryTreeNode<T>> {
public:
    using Node = BinaryTreeNode<T>;
    using MySelf = BinaryTreeForwardIterator<T>;
    using Parent = GeneralIterator<MySelf, Node>;
    using Parent::Parent;
    // 
    MySelf& operator++() {
    auto node = Parent::m_ptr;

    if (node == nullptr)
        return *this;

    if (node->m_pChild[1]) {
        node = node->m_pChild[1];

        while (node->m_pChild[0])
            node = node->m_pChild[0];

        Parent::m_ptr = node;
    } else {
        auto parent = node->m_parent;

        while (parent && node == parent->m_pChild[1]) {
            node = parent;
            parent = parent->m_parent;
        }

        Parent::m_ptr = parent;
    }

    return *this;
}
};
//
template <typename T>
class BinaryTreeBackwardIterator
    : public GeneralIterator<BinaryTreeBackwardIterator<T>, BinaryTreeNode<T>> {
public:
    using Node = BinaryTreeNode<T>;
    using MySelf = BinaryTreeBackwardIterator<T>;
    using Parent = GeneralIterator<MySelf, Node>;
    using Parent::Parent;

    //
    MySelf& operator++() {
    auto node = Parent::m_ptr;

    if (node == nullptr)
        return *this;

    if (node->m_pChild[0]) {
        node = node->m_pChild[0];

        while (node->m_pChild[1])
            node = node->m_pChild[1];

        Parent::m_ptr = node;
    } else {
        auto parent = node->m_parent;

        while (parent && node == parent->m_pChild[0]) {
            node = parent;
            parent = parent->m_parent;
        }

        Parent::m_ptr = parent;
    }

    return *this;
}
};



//

template <typename T>
struct BinaryTreeTraits {
    using value_type = T;
    using Node = BinaryTreeNode<T>;
    using ForwardIterator = BinaryTreeForwardIterator<T>;
       using BackwardIterator = BinaryTreeBackwardIterator<T>;
};
//
template <typename Traits>
class BinaryTree : public GeneralContainer<BinaryTree<Traits>>  {
public:
    using value_type        = Traits::value_type;
    using Node              = Traits::Node;
    using NodePtr = Node*;
    using ForwardIterator   = Traits::ForwardIterator;
    using BackwardIterator  = Traits::BackwardIterator;


    BackwardIterator rbegin() const {
    NodePtr node = m_root;

    while (node && node->m_pChild[1])
        node = node->m_pChild[1];

    return BackwardIterator(node);
}

BackwardIterator rend() const {
    return BackwardIterator(nullptr);
}


private:
    NodePtr m_root = nullptr; // root of the binary tree
    void internal_insert(NodePtr& node,
                     const value_type& value,
                     Ref ref,
                     NodePtr parent = nullptr) {
    if (!node) {
        node = new Node(value, ref);
        node->m_parent = parent;
        return;
    }

    auto branch = value > node->getValue();
    internal_insert(node->m_pChild[branch], value, ref, node);
}

    //
        void internal_clear(NodePtr& node) {
        if (node == nullptr)
            return;

        internal_clear(node->m_pChild[0]);
        internal_clear(node->m_pChild[1]);

        delete node;
        node = nullptr;
    }
    //
    NodePtr internal_copy(const Node* node, NodePtr parent = nullptr) {
    if (node == nullptr)
        return nullptr;

    NodePtr copy = new Node(node->getValue(), node->getRef());
    copy->m_parent = parent;

    try {
        copy->m_pChild[0] = internal_copy(node->m_pChild[0], copy);
        copy->m_pChild[1] = internal_copy(node->m_pChild[1], copy);
    } catch (...) {
        internal_clear(copy);
        throw;
    }

    return copy;
}
//constructors
public:
    BinaryTree() = default;

    BinaryTree(const BinaryTree& other)
        : m_root(internal_copy(other.m_root)) {}

    BinaryTree(BinaryTree&& other) noexcept
         : m_root(std::exchange(other.m_root, nullptr)) {}

    //destructor
    ~BinaryTree() {
    internal_clear(m_root);
}




public:
    void insert(const value_type& value, Ref ref) {
        
        // Implement insertion logic for binary tree
        internal_insert(m_root, value, ref);
    }
    //
    void clear() {
    internal_clear(m_root);
}

    // 
    ForwardIterator begin() const {
    NodePtr node = m_root;

    while (node && node->m_pChild[0])
        node = node->m_pChild[0];

    return ForwardIterator(node);
}

ForwardIterator end() const {
    return ForwardIterator(nullptr);
}
    // Move constructor and move assignment operator
    // Constructor and destructor
    // Persistencia (operator << y operator >> globales para todas las estructuras pero si es para BT con indentacion entonces hacerlo por separado)
    // Concurrencia
    // Iteradores (forward y backward)
    // FirstThat, ForEach, call, rcall por herencia
};
#endif // __BINARYTREE_H__