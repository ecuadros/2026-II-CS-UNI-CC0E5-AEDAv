#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__

#include "GeneralNode.h"
#include "traits.h"
#include "traversalops.h"
#include <algorithm>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <ostream>
#include <type_traits>
#include <utility>

template <typename T, typename Derived, typename R>
class BinaryTreeNode : public GeneralNode<T, R> {
public:
    using Parent = GeneralNode<T, R>;
    using Parent::Parent;
    using Node = NodeSelf<BinaryTreeNode<T, void, R>, Derived>;
    using NodePtr = Node*;

    NodePtr m_pChild[2] = {nullptr, nullptr};
    NodePtr m_pParent = nullptr;
};

template <typename Traits, typename Ops = SearchTraversalOps> class BinaryTree : public Ops {
public:
    using value_type = typename Traits::value_type;
    using Node = typename Traits::Node;
    using ref_type = typename Traits::ref_type;
    using Compare = typename Traits::Compare;
    using NodePtr = Node*;
    using ForwardIterator = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;

    template <TreeOrder Order, bool Reverse>
    using Traversal = TreeTraversal<BinaryTreeIterator<const Node, Order, Reverse>>;

private:
    NodePtr m_root = nullptr; // root of the binary tree
    std::size_t m_size = 0;
    Compare m_comp;

    bool equivalent(const value_type& a, const value_type& b) const {
        return !m_comp(a, b) && !m_comp(b, a);
    }

    std::pair<NodePtr, bool> internal_insert(const value_type& value, const ref_type& ref, bool unique) {
        NodePtr* slot = &m_root;
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

    NodePtr find_node(const value_type& value) const {
        NodePtr node = m_root;
        while (node && !equivalent(value, node->getValue()))
            node = node->m_pChild[m_comp(node->getValue(), value)];
        return node;
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
        return node->m_pParent ? node->m_pParent->m_pChild[side_of(node)] : m_root;
    }

    virtual void take_place(NodePtr, const Node*) {}

    void detach(NodePtr node) {
        NodePtr child = node->m_pChild[0] ? node->m_pChild[0] : node->m_pChild[1];
        link_of(node) = child;
        if (child) child->m_pParent = node->m_pParent;
    }

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

    BinaryTree(std::initializer_list<std::pair<value_type, ref_type>> values) { fill_from(*this, values); }

    BinaryTree(const BinaryTree& other) {
        std::lock_guard lock(other.m_mutex);
        m_root = clone(other.m_root);
        m_size = other.m_size;
    }

    BinaryTree(BinaryTree&& other) noexcept { swap(other); }

    BinaryTree& operator=(BinaryTree other) noexcept {
        swap(other);
        return *this;
    }

    void swap(BinaryTree& other) noexcept {
        if (this == &other) return;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        std::swap(m_root, other.m_root);
        std::swap(m_size, other.m_size);
    }

    virtual ~BinaryTree() { clear(); }

    void insert(const value_type& value, const ref_type& ref) {
        std::lock_guard lock(this->m_mutex);
        internal_insert(value, ref, false);
    }

    std::pair<value_type*, bool> insert_unique(const value_type& value, const ref_type& ref) {
        std::lock_guard lock(this->m_mutex);
        auto [node, inserted] = internal_insert(value, ref, true);
        return {&node->value(), inserted};
    }

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

    template <typename Self> auto find(this Self& self, const value_type& value) {
        using Ptr = std::conditional_t<std::is_const_v<Self>, const value_type*, value_type*>;
        std::lock_guard lock(self.m_mutex);
        NodePtr node = self.find_node(value);
        return node ? Ptr(&node->value()) : nullptr;
    }

    ref_type& operator[](const value_type& value) {
        std::lock_guard lock(this->m_mutex);
        return internal_insert(value, ref_type{}, true).first->ref();
    }

    template <typename Self> auto find_ref(this Self& self, const value_type& value) {
        using Ptr = std::conditional_t<std::is_const_v<Self>, const ref_type*, ref_type*>;
        std::lock_guard lock(self.m_mutex);
        NodePtr node = self.find_node(value);
        return node ? Ptr(&node->ref()) : nullptr;
    }

    bool contains(const value_type& value) const {
        std::lock_guard lock(this->m_mutex);
        return find_node(value) != nullptr;
    }

    void push_back(const value_type& value, const ref_type& ref) { insert(value, ref); }
    bool empty() const { return size() == 0; }

    std::size_t size() const {
        std::lock_guard lock(this->m_mutex);
        return m_size;
    }

    std::size_t height() const {
        std::lock_guard lock(this->m_mutex);
        return height(m_root);
    }

    void clear() {
        std::lock_guard lock(this->m_mutex);
        destroy(std::exchange(m_root, nullptr));
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
