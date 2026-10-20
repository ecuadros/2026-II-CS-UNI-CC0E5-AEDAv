#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__

#include "GeneralNode.h"
#include "treeiterator.h"
#include "traversalops.h"
#include "container_stream.h"
#include <cstddef>
#include <initializer_list>
#include <mutex>
#include <string>
#include <utility>

template <typename T> class BinaryTreeNode : public GeneralNode<T> {
public:
    using Parent = GeneralNode<T>;
    using Parent::Parent;
    using NodePtr = BinaryTreeNode<T>*;

    NodePtr m_pChild[2] = {nullptr, nullptr}; // left and right children
};

template <typename T> struct BinaryTreeTraits {
    using value_type = T;
    using Node = BinaryTreeNode<T>;
    using ForwardIterator = BinaryTreeIterator<Node>;
    using BackwardIterator = BinaryTreeIterator<Node, true>;
};

template <typename Traits> class BinaryTree : public SearchTraversalOps<BinaryTree<Traits>> {
    friend class CallOp<BinaryTree>;
    friend class FirstThatOp<BinaryTree>;

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
        if (!node) {
            node = new Node(value, ref);
            ++m_size;
            return;
        }
        auto branch = value > node->getValue();
        internal_insert(node->m_pChild[branch], value, ref);
    }

    void destroy(NodePtr node) {
        if (!node) return;
        destroy(node->m_pChild[0]);
        destroy(node->m_pChild[1]);
        delete node;
    }

    NodePtr clone(NodePtr source) {
        if (!source) return nullptr;
        NodePtr copy = new Node(source->getValue(), source->getRef());
        try {
            copy->m_pChild[0] = clone(source->m_pChild[0]);
            copy->m_pChild[1] = clone(source->m_pChild[1]);
        } catch (...) {
            destroy(copy);
            throw;
        }
        return copy;
    }

    void printIndented(std::ostream& os, NodePtr node, std::size_t depth) const {
        if (!node) return;
        printIndented(os, node->m_pChild[1], depth + 1);
        os << std::string(depth * 4, ' ') << *node << '\n';
        printIndented(os, node->m_pChild[0], depth + 1);
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
        std::swap(m_root, copy.m_root);
        std::swap(m_size, copy.m_size);
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

private:
    void writePreOrder(std::ostream& os, NodePtr node, bool& first) const {
        if (!node) return;
        if (!first) os << ", ";
        os << *node;
        first = false;
        writePreOrder(os, node->m_pChild[0], first);
        writePreOrder(os, node->m_pChild[1], first);
    }

public:
    std::ostream& write(std::ostream& os) const {
        std::lock_guard lock(m_mutex);
        bool first = true;
        os << '[';
        writePreOrder(os, m_root, first);
        return os << ']';
    }

    std::istream& read(std::istream& is) {
        std::vector<std::pair<value_type, Ref>> values;
        if (!container_stream::read(is, values)) return is;
        BinaryTree parsed;
        for (const auto& [value, ref] : values) parsed.insert(value, ref);
        std::lock_guard lock(m_mutex);
        std::swap(m_root, parsed.m_root);
        std::swap(m_size, parsed.m_size);
        return is;
    }

    void printIndented(std::ostream& os) const {
        std::lock_guard lock(m_mutex);
        printIndented(os, m_root, 0);
    }
};

#endif // __BINARYTREE_H__
