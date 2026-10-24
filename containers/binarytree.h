#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__

#include "GeneralNode.h"
#include "traits.h"
#include "traversalops.h"
#include "container_stream.h"
#include <cstddef>
#include <initializer_list>
#include <mutex>
#include <utility>

template <typename T> class BinaryTreeNode : public GeneralNode<T> {
public:
    using Parent = GeneralNode<T>;
    using Parent::Parent;
    using NodePtr = BinaryTreeNode<T>*;

    const BinaryTreeNode<T>* child(bool right) const { return m_pChild[right]; }

private:
    template <typename> friend class BinaryTree;
    template <typename, bool> friend class BinaryTreeIterator;

    NodePtr m_pChild[2] = {nullptr, nullptr};
    NodePtr m_pParent = nullptr;
};

template <typename Traits> class BinaryTree : public SearchTraversalOps {
    friend class CallOp;
    friend class RCallOp;
    friend class FirstThatOp;
    friend class RFirstThatOp;

public:
    using container_stream_tag = void;
    using value_type = typename Traits::value_type;
    using Node = typename Traits::Node;
    using NodePtr = Node*;
    using ForwardIterator = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
private:
    NodePtr m_root = nullptr; // root of the binary tree
    std::size_t m_size = 0;
    mutable std::mutex m_mutex;

    void internal_insert(NodePtr& node, const value_type& value, Ref ref) {
        NodePtr* slot = &node;
        NodePtr parent = nullptr;
        while (*slot) {
            parent = *slot;
            slot = &(*slot)->m_pChild[value > (*slot)->getValue()];
        }
        *slot = new Node(value, ref);
        (*slot)->m_pParent = parent;
        ++m_size;
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
        NodePtr copy = new Node(source->getValue(), source->getRef());
        copy->m_pParent = parent;
        try {
            copy->m_pChild[0] = clone(source->m_pChild[0], copy);
            copy->m_pChild[1] = clone(source->m_pChild[1], copy);
        } catch (...) {
            destroy(copy);
            throw;
        }
        return copy;
    }

    void writeNodes(std::ostream& os, NodePtr node, bool& first) const {
        if (!node) return;
        if (!first) os << ',';
        first = false;
        os << *node;
        writeNodes(os, node->m_pChild[0], first);
        writeNodes(os, node->m_pChild[1], first);
    }

public:
    BinaryTree() = default;

    BinaryTree(std::initializer_list<std::pair<value_type, Ref>> values) {
        try {
            for (const auto& [value, ref] : values)
                insert(value, ref);
        } catch (...) {
            clear();
            throw;
        }
    }

    BinaryTree(const BinaryTree& other) {
        std::lock_guard lock(other.m_mutex);
        m_root = clone(other.m_root);
        m_size = other.m_size;
    }

    BinaryTree& operator=(const BinaryTree& other) {
        if (this == &other) return *this;
        BinaryTree copy(other);
        std::lock_guard lock(m_mutex);
        swapState(copy);
        return *this;
    }

    BinaryTree(BinaryTree&& other) {
        std::lock_guard lock(other.m_mutex);
        m_root = std::exchange(other.m_root, nullptr);
        m_size = std::exchange(other.m_size, 0);
    }

    BinaryTree& operator=(BinaryTree&& other) {
        if (this == &other) return *this;
        std::scoped_lock lock(m_mutex, other.m_mutex);
        destroy(m_root);
        m_root = std::exchange(other.m_root, nullptr);
        m_size = std::exchange(other.m_size, 0);
        return *this;
    }

    ~BinaryTree() { clear(); }

    void insert(const value_type& value, Ref ref) {
        std::lock_guard lock(m_mutex);
        internal_insert(m_root, value, ref);
    }

    void push_back(const value_type& value, Ref ref) { insert(value, ref); }
    bool empty() const { return size() == 0; }

    std::size_t size() const {
        std::lock_guard lock(m_mutex);
        return m_size;
    }

    void clear() {
        std::lock_guard lock(m_mutex);
        destroy(m_root);
        m_root = nullptr;
        m_size = 0;
    }

    ForwardIterator begin() const { return ForwardIterator(m_root); }
    ForwardIterator end() const { return ForwardIterator(nullptr); }
    BackwardIterator rbegin() const { return BackwardIterator(m_root); }
    BackwardIterator rend() const { return BackwardIterator(nullptr); }

    std::ostream& write(std::ostream& os) const {
        std::lock_guard lock(m_mutex);
        os << '[';
        bool first = true;
        writeNodes(os, m_root, first);
        return os << ']';
    }

    std::istream& read(std::istream& is) {
        BinaryTree parsed;
        if (!container_stream::read(is, parsed)) return is;
        std::lock_guard lock(m_mutex);
        swapState(parsed);
        return is;
    }

};

#endif // __BINARYTREE_H__
