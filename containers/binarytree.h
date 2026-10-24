#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include <initializer_list>
#include <mutex>
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralTraits.h"
#include "containers.h"
#include "../persistence.h"

template <typename T>
class BinaryTreeNode : public GeneralNode<T> {
public:
    using value_type        = T;
    using MySelf            = BinaryTreeNode<T>;
    using Node              = MySelf;
    using NodePtr           = Node*;
    using Parent            = GeneralNode<T>;
    using Parent::Parent; // Inherit constructor

    NodePtr m_pChild[2] = {nullptr, nullptr}; // left and right children
    NodePtr m_pParent   = nullptr;
};

template <typename T, bool _Branch>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<T,_Branch>, BinaryTreeNode<T>> {
public:
    using Node      = BinaryTreeNode<T>;
    using value_type = Node;
    using MySelf    = BinaryTreeIterator<T, _Branch>;
    using Parent    = GeneralIterator<MySelf, value_type>;
    using Parent::Parent; // Inherit constructor

    BinaryTreeIterator& operator++() {
        Node* current = Parent::m_ptr;
        if (!current) return *this;

        if (current->m_pChild[!_Branch]) {
            current = current->m_pChild[!_Branch];
            while (current->m_pChild[_Branch])
                current = current->m_pChild[_Branch];
        } else {
            Node* parent = current->m_pParent;
            while (parent && current == parent->m_pChild[!_Branch]) {
                current = parent;
                parent  = parent->m_pParent;
            }
            current = parent;
        }
        Parent::m_ptr = current;
        return *this;
    }
};

// Recorrido left - node - right.
template <typename T, bool _Branch = 0>
class BinaryTreeForwardIterator : public BinaryTreeIterator<T, _Branch> {};

// Recorrido  right - node - left.
template <typename T, bool _Branch = 1>
class BinaryTreeBackwardIterator : public BinaryTreeIterator<T, _Branch> {};


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
class BinaryTree : public Firstable<BinaryTree<Traits>, Traits>, public Recallable<BinaryTree<Traits>, Traits> {
public:
    using value_type        = typename Traits::value_type;
    using Node              = typename Traits::Node;
    using NodePtr           = Node *;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using BackwardIterator  = typename Traits::BackwardIterator;
    using Compare           = typename Traits::Compare;

private:
    NodePtr m_root = nullptr; // root of the binary tree

    void internal_insert(NodePtr& node, NodePtr parent, const value_type& value, Ref ref) {
        if (!node) {
            node = new Node(value, ref);
            node->m_pParent = parent;
            return;
        }

        auto branch = Compare()(value, node->getValue()) ? 0 : 1;
        internal_insert(node->m_pChild[branch], node, value, ref);
    }

    void internal_clear(NodePtr node) {
        if (!node) return;
        internal_clear(node->m_pChild[0]);
        internal_clear(node->m_pChild[1]);
        delete node;
    }

    static NodePtr first(NodePtr node) {
        while (node && node->m_pChild[0])
            node = node->m_pChild[0];
        return node;
    }

    static NodePtr last(NodePtr node) {
        while (node && node->m_pChild[1])
            node = node->m_pChild[1];
        return node;
    }

public:
    using Delim = typename Node::Delim;
    BinaryTree() {}

    BinaryTree(initializer_list<pair<value_type, Ref>> values) {
        for (const auto &v : values)
            insert(v.first, v.second);
    }

    //Copia
    BinaryTree(const BinaryTree& another) {
        *this = another;
    }

    //Copia
    BinaryTree& operator=(const BinaryTree& another) {
        std::scoped_lock lock(this->m_mutex, another.m_mutex);
        internal_clear(m_root);
        for (const auto& node : another)
            push_back(node.getValue(), node.getRef());
        return *this;
    }

    //Move
    BinaryTree(BinaryTree&& another) noexcept {
        *this = another;
    }

    //Move
    BinaryTree& operator=(BinaryTree&& another) noexcept {
        std::scoped_lock lock(this->m_mutex, another.m_mutex);
        internal_clear(m_root);
        m_root = another.m_root;
        another.m_root = nullptr;
        return *this;
    }

    virtual ~BinaryTree() { clear(); }

    void insert(const value_type& value, Ref ref) {
        scoped_lock lock(this->m_mutex);
        internal_insert(m_root, nullptr, value, ref);
    }

    void push(const value_type& value, Ref ref) { insert(value, ref); }

    void clear() {
        scoped_lock lock(this->m_mutex);
        internal_clear(m_root);
        m_root = nullptr;
    }

    bool empty() const { return m_root == nullptr; }

    std::ostream& write(std::ostream& os) { return os << *this; }
    std::istream& read(std::istream& is) { return is >> *this; }

    ForwardIterator begin() override { return ForwardIterator(first(m_root)); }
    ForwardIterator end()   override { return ForwardIterator(nullptr); }

    BackwardIterator rbegin() override { return BackwardIterator(last(m_root)); }
    BackwardIterator rend()   override { return BackwardIterator(nullptr); }

    using Callable<BinaryTree<Traits>, Traits>::call;
    using Callable<BinaryTree<Traits>, Traits>::m_mutex;
    using Firstable<BinaryTree<Traits>, Traits>::FirstThat;
    using Recallable<BinaryTree<Traits>, Traits>::rcall;

    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args&&... args) {
        this->call(func, std::forward<Args>(args)...);
    }

    // Move constructor and move assignment operator
    // Persistencia (operator << y operator >> globales para todas las estructuras pero si es para BT con indentacion entonces hacerlo por separado)
    // FirstThat, ForEach, call, rcall por herencia
};

#endif // __BINARYTREE_H__
