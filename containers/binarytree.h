#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralContainer.h"
#include "stream.h"
#include "traits.h"

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

};

template <typename Node, size_t First = 0>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<Node, First>, Node> {
public:
    using value_type = Node;
    using MySelf = BinaryTreeIterator<Node, First>;
    using Parent = GeneralIterator<MySelf, value_type>;
private:
    vector<Node*> m_pending;

    void pushBranch(Node* node) {
        for (; node; node = node->m_pChild[First])
            m_pending.push_back(node);
        Parent::m_ptr = m_pending.empty() ? nullptr : m_pending.back();
    }
public:
    BinaryTreeIterator(Node* root) : Parent(nullptr) { pushBranch(root); }
    BinaryTreeIterator& operator++() {
        Node* node = m_pending.back();
        m_pending.pop_back();
        pushBranch(node->m_pChild[1 - First]);
        return *this;
    }
};

template <typename T>
struct BinaryTreeStructure {
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeIterator<Node>;
    using BackwardIterator  = BinaryTreeIterator<Node, 1>;
};

template <typename T>
using BinaryTreeAscTraits = ListTraits<AscendingTraits<T>, BinaryTreeStructure<T>>;

template <typename T>
using BinaryTreeDescTraits = ListTraits<DescendingTraits<T>, BinaryTreeStructure<T>>;

template <typename Traits, typename Derived = void>
class BinaryTree : public GeneralContainer<conditional_t<is_void_v<Derived>, BinaryTree<Traits>, Derived>> {
    using Base = GeneralContainer<conditional_t<is_void_v<Derived>, BinaryTree<Traits>, Derived>>;
    friend Base;
public:
    using value_type        = Traits::value_type;
    using Node              = Traits::Node;
    using NodePtr           = Node*;
    using ForwardIterator   = Traits::ForwardIterator;
    using BackwardIterator  = Traits::BackwardIterator;
    using Compare           = Traits::Compare;

private:
    NodePtr m_pRoot = nullptr;
    size_t  m_size  = 0;
    Compare m_comp;
    using Base::m_mutex;

    void internalInsert(NodePtr node);
    void internalClear();
    void copyFrom(const BinaryTree& another);
    void moveFrom(BinaryTree& another);
    template <typename Func>
    void preorder(Func visit) const;

protected:
    void AfterApply();

public:
    BinaryTree() {}
    BinaryTree(const BinaryTree& another) : BinaryTree() { copyFrom(another); }
    BinaryTree& operator=(const BinaryTree& another) { copyFrom(another); return *this; }
    BinaryTree(BinaryTree&& another) noexcept : BinaryTree() { moveFrom(another); }
    BinaryTree& operator=(BinaryTree&& another) noexcept { moveFrom(another); return *this; }
    virtual ~BinaryTree() { clear(); }

    void insert(const value_type& value, Ref ref) {
        lock_guard<mutex> lock(m_mutex);
        internalInsert(new Node(value, ref));
        ++m_size;
    }
    void push_back(const value_type& value, Ref ref) { insert(value, ref); }

    void clear() {
        lock_guard<mutex> lock(m_mutex);
        internalClear();
    }

    size_t size()  const { lock_guard<mutex> lock(m_mutex); return m_size; }
    bool   empty() const { lock_guard<mutex> lock(m_mutex); return m_pRoot == nullptr; }

    ForwardIterator  begin()  const { return ForwardIterator(m_pRoot); }
    ForwardIterator  end()    const { return ForwardIterator(nullptr); }
    BackwardIterator rbegin() const { return BackwardIterator(m_pRoot); }
    BackwardIterator rend()   const { return BackwardIterator(nullptr); }

    std::ostream& write(std::ostream& os) const;
    std::istream& read(std::istream& is) { return ReadContainer(is, *this); }
    std::ostream& print(std::ostream& os) const;
};

template <typename Traits, typename Derived>
void BinaryTree<Traits, Derived>::internalInsert(NodePtr node) {
    NodePtr* link = &m_pRoot;
    while (*link) {
        auto branch = !m_comp(node->getValue(), (*link)->getValue());
        link = &(*link)->m_pChild[branch];
    }
    *link = node;
}

template <typename Traits, typename Derived>
void BinaryTree<Traits, Derived>::internalClear() {
    NodePtr node = m_pRoot;
    m_pRoot = nullptr;
    m_size = 0;
    while (node) {
        NodePtr left = node->m_pChild[0];
        if (left) {
            node->m_pChild[0] = left->m_pChild[1];
            left->m_pChild[1] = node;
            node = left;
        } else {
            NodePtr right = node->m_pChild[1];
            delete node;
            node = right;
        }
    }
}

template <typename Traits, typename Derived>
void BinaryTree<Traits, Derived>::copyFrom(const BinaryTree& another) {
    if (this == &another)
        return;
    scoped_lock lock(m_mutex, another.m_mutex);
    internalClear();
    m_comp = another.m_comp;
    vector<pair<NodePtr, NodePtr*>> pending{{another.m_pRoot, &m_pRoot}};
    while (!pending.empty()) {
        auto [source, link] = pending.back();
        pending.pop_back();
        if (!source)
            continue;
        *link = new Node(source->getValue(), source->getRef());
        ++m_size;
        pending.push_back({source->m_pChild[1], &(*link)->m_pChild[1]});
        pending.push_back({source->m_pChild[0], &(*link)->m_pChild[0]});
    }
}

template <typename Traits, typename Derived>
void BinaryTree<Traits, Derived>::moveFrom(BinaryTree& another) {
    if (this == &another)
        return;
    scoped_lock lock(m_mutex, another.m_mutex);
    internalClear();
    m_comp  = another.m_comp;
    m_pRoot = std::exchange(another.m_pRoot, nullptr);
    m_size  = std::exchange(another.m_size, 0);
}

template <typename Traits, typename Derived>
template <typename Func>
void BinaryTree<Traits, Derived>::preorder(Func visit) const {
    vector<NodePtr> pending{m_pRoot};
    while (!pending.empty()) {
        NodePtr node = pending.back();
        pending.pop_back();
        if (!node)
            continue;
        visit(node);
        pending.push_back(node->m_pChild[1]);
        pending.push_back(node->m_pChild[0]);
    }
}

template <typename Traits, typename Derived>
void BinaryTree<Traits, Derived>::AfterApply() {
    NodePtr prev = nullptr;
    auto sorted = true;
    for (auto it = begin(); sorted && it != end(); ++it) {
        if (prev && m_comp((*it).getValue(), prev->getValue()))
            sorted = false;
        prev = &*it;
    }
    if (sorted)
        return;
    vector<NodePtr> nodes;
    preorder([&](NodePtr node) { nodes.push_back(node); });
    m_pRoot = nullptr;
    for (NodePtr node : nodes) {
        node->m_pChild[0] = node->m_pChild[1] = nullptr;
        internalInsert(node);
    }
}

template <typename Traits, typename Derived>
std::ostream& BinaryTree<Traits, Derived>::write(std::ostream& os) const {
    lock_guard<mutex> lock(m_mutex);
    auto first = true;
    os << "[";
    preorder([&](NodePtr node) {
        if (!first)
            os << ",";
        os << *node;
        first = false;
    });
    return os << "]";
}

template <typename Traits, typename Derived>
std::ostream& BinaryTree<Traits, Derived>::print(std::ostream& os) const {
    lock_guard<mutex> lock(m_mutex);
    vector<pair<NodePtr, size_t>> pending;
    NodePtr node = m_pRoot;
    size_t depth = 0;
    while (node || !pending.empty()) {
        for (; node; node = node->m_pChild[1], ++depth)
            pending.push_back({node, depth});
        node  = pending.back().first;
        depth = pending.back().second;
        pending.pop_back();
        os << std::string(4 * depth, ' ') << *node << "\n";
        node = node->m_pChild[0];
        ++depth;
    }
    return os;
}

#endif // __BINARYTREE_H__