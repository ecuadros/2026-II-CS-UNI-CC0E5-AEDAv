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
#include <utility>

template <typename T, typename Derived = void>
class BinaryTreeNode : public GeneralNode<T> {
public:
    using Parent = GeneralNode<T>;
    using Parent::Parent;
    using Node = NodeSelf<BinaryTreeNode<T>, Derived>;
    using NodePtr = Node*;

    NodePtr m_pChild[2] = {nullptr, nullptr};
    NodePtr m_pParent = nullptr;
};

template <typename Traits> class BinaryTree : public SearchTraversalOps {
public:
    using value_type = typename Traits::value_type;
    using Node = typename Traits::Node;
    using NodePtr = Node*;
    using ForwardIterator = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;

    template <TreeOrder Order, bool Reverse>
    using Traversal = TreeTraversal<BinaryTreeIterator<const Node, Order, Reverse>>;

private:
    NodePtr m_root = nullptr; // root of the binary tree
    std::size_t m_size = 0;

    std::pair<NodePtr, bool> internal_insert(const value_type& value, Ref ref, bool unique) {
        NodePtr* slot = &m_root;
        NodePtr parent = nullptr;
        while (*slot) {
            parent = *slot;
            if (unique && value == parent->getValue()) return {parent, false};
            slot = &parent->m_pChild[value > parent->getValue()];
        }
        NodePtr node = new Node(value, ref);
        node->m_pParent = parent;
        *slot = node;
        ++m_size;
        after_insert(node);
        return {node, true};
    }

    NodePtr find_node(const value_type& value) const {
        NodePtr node = m_root;
        while (node && node->getValue() != value)
            node = node->m_pChild[value > node->getValue()];
        return node;
    }

    void swapState(BinaryTree& other) {
        std::swap(m_root, other.m_root);
        std::swap(m_size, other.m_size);
    }

    void destroy(NodePtr node) {
        if (!node) return;
        destroy(node->m_pChild[0]);
        destroy(node->m_pChild[1]);
        delete node;
    }

    NodePtr clone(NodePtr source, NodePtr parent = nullptr) {
        if (!source) return nullptr;
        NodePtr copy = new Node(*source);
        copy->m_pParent = parent;
        copy->m_pChild[0] = copy->m_pChild[1] = nullptr;
        try {
            copy->m_pChild[0] = clone(source->m_pChild[0], copy);
            copy->m_pChild[1] = clone(source->m_pChild[1], copy);
        } catch (...) {
            destroy(copy);
            throw;
        }
        return copy;
    }

    template <TreeOrder Order, bool Reverse> Traversal<Order, Reverse> traversal() const {
        return Traversal<Order, Reverse>(m_root);
    }

protected:
    virtual void after_insert(NodePtr) {}
    virtual void after_remove(NodePtr, bool) {}
    virtual NodePtr replacement(NodePtr node) const { return extreme(node->m_pChild[1], 0); }
    virtual void print_node(std::ostream& os, const Node& node) const { os << node; }

    virtual std::size_t height(const Node* node) const {
        if (!node) return 0;
        return 1 + std::max(height(node->m_pChild[0]), height(node->m_pChild[1]));
    }

    NodePtr& link_of(NodePtr node) {
        NodePtr parent = node->m_pParent;
        if (!parent) return m_root;
        return parent->m_pChild[parent->m_pChild[1] == node];
    }

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
    BinaryTree() = default;

    BinaryTree(std::initializer_list<std::pair<value_type, Ref>> values) { fill_from(*this, values); }

    BinaryTree(const BinaryTree& other) {
        std::lock_guard lock(other.m_mutex);
        m_root = clone(other.m_root);
        m_size = other.m_size;
    }

    BinaryTree(BinaryTree&& other) {
        std::lock_guard lock(other.m_mutex);
        swapState(other);
    }

    BinaryTree& operator=(BinaryTree other) {
        std::lock_guard lock(m_mutex);
        swapState(other);
        return *this;
    }

    virtual ~BinaryTree() { clear(); }

    void insert(const value_type& value, Ref ref) {
        std::lock_guard lock(m_mutex);
        internal_insert(value, ref, false);
    }

    std::pair<value_type*, bool> insert_unique(const value_type& value, Ref ref) {
        std::lock_guard lock(m_mutex);
        auto [node, inserted] = internal_insert(value, ref, true);
        return {&node->value(), inserted};
    }

    bool remove(const value_type& value) {
        std::lock_guard lock(m_mutex);
        NodePtr node = find_node(value);
        if (!node) return false;
        if (node->m_pChild[0] && node->m_pChild[1]) {
            NodePtr repl = replacement(node);
            static_cast<GeneralNode<value_type>&>(*node) = *repl;
            node = repl;
        }
        NodePtr child = node->m_pChild[0] ? node->m_pChild[0] : node->m_pChild[1];
        NodePtr parent = node->m_pParent;
        bool side = parent && parent->m_pChild[1] == node;
        link_of(node) = child;
        if (child) child->m_pParent = parent;
        delete node;
        --m_size;
        after_remove(parent, side);
        return true;
    }

    value_type* find(const value_type& value) {
        std::lock_guard lock(m_mutex);
        NodePtr node = find_node(value);
        return node ? &node->value() : nullptr;
    }

    const value_type* find(const value_type& value) const {
        std::lock_guard lock(m_mutex);
        NodePtr node = find_node(value);
        return node ? &node->value() : nullptr;
    }

    bool contains(const value_type& value) const {
        std::lock_guard lock(m_mutex);
        return find_node(value) != nullptr;
    }

    void push_back(const value_type& value, Ref ref) { insert(value, ref); }
    bool empty() const { return size() == 0; }

    std::size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_size;
    }

    std::size_t height() const {
        std::lock_guard lock(m_mutex);
        return height(m_root);
    }

    void clear() {
        std::lock_guard lock(m_mutex);
        destroy(m_root);
        m_root = nullptr;
        m_size = 0;
    }

    struct View {
        using Handle = const Node*;
        const BinaryTree* tree;

        Handle root() const { return tree->m_root; }
        Handle child(Handle node, bool side) const { return node->m_pChild[side]; }
        bool valid(Handle node) const { return node != nullptr; }
        void label(std::ostream& os, Handle node) const { tree->print_node(os, *node); }
        std::size_t height(Handle node) const { return tree->height(node); }
    };

    View tree_view() const { return View{this}; }

    ForwardIterator begin() const { return ForwardIterator(m_root); }
    ForwardIterator end() const { return ForwardIterator(nullptr); }
    BackwardIterator rbegin() const { return BackwardIterator(m_root); }
    BackwardIterator rend() const { return BackwardIterator(nullptr); }

    auto InOrder() const { return traversal<TreeOrder::InOrder, false>(); }
    auto ReverseInOrder() const { return traversal<TreeOrder::InOrder, true>(); }
    auto PreOrder() const { return traversal<TreeOrder::PreOrder, false>(); }
    auto ReversePreOrder() const { return traversal<TreeOrder::PostOrder, true>(); }
    auto PostOrder() const { return traversal<TreeOrder::PostOrder, false>(); }
    auto ReversePostOrder() const { return traversal<TreeOrder::PreOrder, true>(); }
};

#endif // __BINARYTREE_H__
