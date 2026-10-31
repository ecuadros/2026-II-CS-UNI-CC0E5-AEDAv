#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralContainer.h"
#include "mixins.h"

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
    NodePtr m_parent = nullptr;

};

enum class TransversarOrder {
    PreOrder,
    InOrder,
    PostOrder
};

template <typename T, TransversarOrder TrOrder, TX Dir>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<T, TrOrder, Dir>, BinaryTreeNode<T>> {
public:
    using Node = BinaryTreeNode<T>;
    using MySelf = BinaryTreeIterator<T, TrOrder, Dir>;
    using Parent = GeneralIterator<MySelf, Node>;
    using NodePtr = Node*;
    using Parent::Parent;
    NodePtr m_parent = nullptr;

    MySelf& operator++();
};

template <typename T, TransversarOrder TrOrder, TX Dir>
auto BinaryTreeIterator<T, TrOrder, Dir>::operator++() -> MySelf& {
    auto node = Parent::m_ptr;
    if (node == nullptr)
        return *this;
    switch (TrOrder) {
        case TransversarOrder::InOrder:
            if (node->m_pChild[1-Dir]) {
                node = node->m_pChild[1-Dir];
                while (node->m_pChild[Dir])
                    node = node->m_pChild[Dir];
                Parent::m_ptr = node;
            }
            else {
                auto parent = node->m_parent;
                while (parent && node == parent->m_pChild[1-Dir]) {
                    node = parent;
                    parent = parent->m_parent;
                }
                Parent::m_ptr = parent;
            }
            return *this;
        case TransversarOrder::PreOrder:
            if (node->m_pChild[Dir]) {
                Parent::m_ptr = node->m_pChild[Dir];
            }
            else if (node->m_pChild[1-Dir]) {
                Parent::m_ptr = node->m_pChild[1-Dir];
            }
            else {
                auto parent = node->m_parent;
                while (parent && (parent->m_pChild[1-Dir] == nullptr || node == parent->m_pChild[1-Dir])) {
                    node = parent;
                    parent = parent->m_parent;
                }
                Parent::m_ptr = parent ? parent->m_pChild[1-Dir] : nullptr;
            }
            return *this;
        case TransversarOrder::PostOrder:
            auto parent = node->m_parent;
            if (!parent) {
                Parent::m_ptr = nullptr;
                return *this;
            }
            if (node == parent->m_pChild[Dir] && parent->m_pChild[1-Dir]) {
                node = parent->m_pChild[1-Dir];
                while (node->m_pChild[Dir] || node->m_pChild[1-Dir]) {
                    if (node->m_pChild[Dir])
                        node = node->m_pChild[Dir];
                    else
                        node = node->m_pChild[1-Dir];
                }
                Parent::m_ptr = node;
            } else {
                Parent::m_ptr = parent;
            }
            return *this;
    }

}

template <typename T, TransversarOrder TrOrder>
using BinaryTreeForwardIterator = BinaryTreeIterator<T, TrOrder, 0>;

template <typename T, TransversarOrder TrOrder>
using BinaryTreeBackwardIterator = BinaryTreeIterator<T, TrOrder, 1>;

template <typename T, TransversarOrder TrOrder>
struct BinaryTreeTraits {
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeForwardIterator<T, TrOrder>;
    using BackwardIterator  = BinaryTreeBackwardIterator<T, TrOrder>;
    using Order             = TransversarOrder;
};

template <typename T, TransversarOrder TrOrder>
struct BinaryTreeAscTraits : public AscendingTraits<T>, public BinaryTreeTraits<T, TrOrder> {};

template <typename T, TransversarOrder TrOrder>
struct BinaryTreeDescTraits : public DescendingTraits<T>, public BinaryTreeTraits<T, TrOrder> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseBinaryTree;

template <typename Traits>
using BinaryTree = BaseBinaryTree<Traits, Callable, Applicable, Searchable, ReverseCallable, Printable, Readable>;

template <typename Traits, template <typename> typename... Mixins>
class BaseBinaryTree : public GeneralContainer<Traits, Mixins...> {
public:
    using MySelf = BaseBinaryTree;
    using Parent = GeneralContainer<Traits, Mixins...>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    //using typename Parent::ForwardIterator;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
    using Compare = typename Traits::Compare;
    using Order = typename Traits::Order;

protected:
    using Parent::m_mutex;

private:
    NodePtr m_root = nullptr; // root of the binary tree
    void internal_insert(NodePtr& node, const value_type& value, Ref ref, NodePtr parent = nullptr);
public:
    BaseBinaryTree() = default;
    BaseBinaryTree(const BaseBinaryTree& another) { *this = another; }
    BaseBinaryTree(BaseBinaryTree&& another) noexcept { *this = std::move(another); }
    virtual ~BaseBinaryTree() { clear(); }

    void insert(const value_type& value, Ref ref) {
        std::lock_guard lock(this->m_mutex);
        internal_insert(m_root, value, ref);
    }
    // Move constructor and move assignment operator
    // Constructor and destructor
    // Persistencia (operator << y operator >> globales para todas las estructuras pero si es para BT con indentacion entonces hacerlo por separado)
    // Concurrencia
    // Iteradores (forward y backward)
    // FirstThat, ForEach, call, rcall por herencia

    void clear() override;

    void push_back(const value_type& value, Ref ref) override { insert(value, ref); }

    void clear_recursive(NodePtr node);

    ForwardIterator begin() const override;
    ForwardIterator end() const override { return ForwardIterator(nullptr); }

    BackwardIterator rbegin() const;
    BackwardIterator rend() const { return BackwardIterator(nullptr); }

    BaseBinaryTree& operator=(const BaseBinaryTree& another);
    BaseBinaryTree& operator=(BaseBinaryTree&& another) noexcept;
};

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::internal_insert(NodePtr& node, const value_type& value, Ref ref, NodePtr parent) {
    if (!node) {
        node = new Node(value, ref);
        node->m_parent = parent;
        return;
    }
    auto branch = Compare()(node->getValue(), value);
    internal_insert(node->m_pChild[branch], value, ref, node); // Tail recursion to insert in the correct branch
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::clear() {
    std::lock_guard lock(this->m_mutex);
    clear_recursive(m_root);
    m_root = nullptr;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::clear_recursive(NodePtr node) {
    if (!node) return;
    clear_recursive(node->m_pChild[0]);
    clear_recursive(node->m_pChild[1]);
    delete node;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::begin() const -> ForwardIterator {
    NodePtr curr = m_root;
    while (curr && curr->m_pChild[0]) curr = curr->m_pChild[0];
    return ForwardIterator(curr);
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::rbegin() const -> BackwardIterator {
    NodePtr curr = m_root;
    while (curr && curr->m_pChild[1]) curr = curr->m_pChild[1];
    return BackwardIterator(curr);
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::operator=(const BaseBinaryTree& another) -> BaseBinaryTree& {
    this->copy_from(another);
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::operator=(BaseBinaryTree&& another) noexcept -> BaseBinaryTree& {
    this->move_from(another, [&]{ m_root = std::exchange(another.m_root, nullptr); });
    return *this;
}

template <typename Traits>
class BinaryTreeInterface {
    private:
        BinaryTree<Traits> bt;
};

#endif // __BINARYTREE_H__
