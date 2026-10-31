#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include <string>
#include <type_traits>
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralContainer.h"
#include "mixins.h"

template <typename T, typename Derived = void>
class BinaryTreeNode : public GeneralNode<T> {
public:
    using value_type        = T;
    using MySelf            = BinaryTreeNode<T, Derived>;
    using Node              = std::conditional_t<std::is_void_v<Derived>, MySelf, Derived>;
    using NodePtr           = Node*;
    using Parent            = GeneralNode<T>;
    using Parent::Parent; // Inherit constructor

    NodePtr m_pChild[2] = {nullptr, nullptr}; // left and right children
    NodePtr m_parent = nullptr;

};

enum class Traversal { PreOrder = 0, InOrder = 1, PostOrder = 2 };

template <typename Node, Traversal Order, TX Dir>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<Node, Order, Dir>, Node> {
public:
    using MySelf = BinaryTreeIterator<Node, Order, Dir>;
    using Parent = GeneralIterator<MySelf, Node>;
    using NodePtr = Node*;

    explicit BinaryTreeIterator(NodePtr root)
        : Parent(root && Order != Traversal::PreOrder ? next(root, 0) : root) {}

    MySelf& operator++() {
        if (Parent::m_ptr)
            Parent::m_ptr = next(Parent::m_ptr, TX(Order));
        return *this;
    }

private:
    static NodePtr next(NodePtr node, TX done);
};

template <typename Node, Traversal Order, TX Dir>
auto BinaryTreeIterator<Node, Order, Dir>::next(NodePtr node, TX done) -> NodePtr {
    do {
        if (done == 2) {
            NodePtr parent = node->m_parent;
            done = parent && node == parent->m_pChild[Dir] ? 1 : 2;
            node = parent;
        } else if (NodePtr child = node->m_pChild[done ? 1-Dir : Dir]) {
            node = child;
            done = 0;
        } else {
            ++done;
        }
    } while (node && done != TX(Order));
    return node;
}

template <typename Node>
using BinaryTreeForwardIterator = BinaryTreeIterator<Node, Traversal::InOrder, 0>;

template <typename Node>
using BinaryTreeBackwardIterator = BinaryTreeIterator<Node, Traversal::InOrder, 1>;

template <typename T, typename _Node = BinaryTreeNode<T>>
struct BinaryTreeTraits {
    using Node              = _Node;
    using ForwardIterator   = BinaryTreeForwardIterator<Node>;
    using BackwardIterator  = BinaryTreeBackwardIterator<Node>;
};

template <typename T>
struct BinaryTreeAscTraits : public AscendingTraits<T>, public BinaryTreeTraits<T> {};

template <typename T>
struct BinaryTreeDescTraits : public DescendingTraits<T>, public BinaryTreeTraits<T> {};

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
    using typename Parent::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
    using Compare = typename Traits::Compare;
    template <Traversal Order, TX Dir>
    using Iterator = BinaryTreeIterator<Node, Order, Dir>;

protected:
    using Parent::m_mutex;
    void transplant(NodePtr node, NodePtr child);
    virtual void rebalance(NodePtr) {}

private:
    NodePtr m_root = nullptr; // root of the binary tree
    NodePtr internal_insert(NodePtr& node, const value_type& value, Ref ref, NodePtr parent = nullptr);
    NodePtr internal_remove(NodePtr node);
    void clear_recursive(NodePtr node);
    NodePtr clone_recursive(NodePtr node, NodePtr parent) const;
    void print_recursive(std::ostream& os, NodePtr node, const std::string& prefix, TX side) const;
public:
    BaseBinaryTree() = default;
    BaseBinaryTree(const BaseBinaryTree& another) { *this = another; }
    BaseBinaryTree(BaseBinaryTree&& another) noexcept { *this = std::move(another); }
    virtual ~BaseBinaryTree() { clear(); }

    void insert(const value_type& value, Ref ref) {
        std::lock_guard lock(this->m_mutex);
        rebalance(internal_insert(m_root, value, ref));
    }
    bool remove(const value_type& value);
    void print(std::ostream& os) const;
    // Move constructor and move assignment operator
    // Constructor and destructor
    // Persistencia (operator << y operator >> globales para todas las estructuras pero si es para BT con indentacion entonces hacerlo por separado)
    // Concurrencia
    // Iteradores (forward y backward)
    // FirstThat, ForEach, call, rcall por herencia

    void clear() override;

    void push_back(const value_type& value, Ref ref) override { insert(value, ref); }

    ForwardIterator begin() const override { return ForwardIterator(m_root); }
    ForwardIterator end() const override { return ForwardIterator(nullptr); }

    BackwardIterator rbegin() const { return BackwardIterator(m_root); }
    BackwardIterator rend() const { return BackwardIterator(nullptr); }

    template <Traversal Order, TX Dir>
    Iterator<Order, Dir> begin() const { return Iterator<Order, Dir>(m_root); }
    template <Traversal Order, TX Dir>
    Iterator<Order, Dir> end() const { return Iterator<Order, Dir>(nullptr); }

    template <Traversal Order, TX Dir, typename Func, typename... Args>
    decltype(auto) traverse(Func func, Args&&... args) {
        std::lock_guard lock(this->m_mutex);
        return ::call(begin<Order, Dir>(), end<Order, Dir>(), func, std::forward<Args>(args)...);
    }

    BaseBinaryTree& operator=(const BaseBinaryTree& another);
    BaseBinaryTree& operator=(BaseBinaryTree&& another) noexcept;
};

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::internal_insert(NodePtr& node, const value_type& value, Ref ref, NodePtr parent) -> NodePtr {
    if (!node) {
        node = new Node(value, ref);
        node->m_parent = parent;
        return node;
    }
    auto branch = Compare()(node->getValue(), value);
    return internal_insert(node->m_pChild[branch], value, ref, node); // Tail recursion to insert in the correct branch
}

template <typename Traits, template <typename> typename... Mixins>
bool BaseBinaryTree<Traits, Mixins...>::remove(const value_type& value) {
    std::lock_guard lock(this->m_mutex);
    NodePtr node = m_root;
    while (node) {
        auto branch = Compare()(node->getValue(), value);
        if (!branch && !Compare()(value, node->getValue()))
            break;
        node = node->m_pChild[branch];
    }
    if (!node)
        return false;
    rebalance(internal_remove(node));
    return true;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::internal_remove(NodePtr node) -> NodePtr {
    NodePtr left = node->m_pChild[0], right = node->m_pChild[1];
    NodePtr replacement = left ? left : right;
    NodePtr changed = node->m_parent;
    if (left && right) {
        while (replacement->m_pChild[1])
            replacement = replacement->m_pChild[1];
        changed = replacement;
        if (replacement != left) {
            changed = replacement->m_parent;
            transplant(replacement, replacement->m_pChild[0]);
            replacement->m_pChild[0] = left;
            left->m_parent = replacement;
        }
        replacement->m_pChild[1] = right;
        right->m_parent = replacement;
    }
    transplant(node, replacement);
    delete node;
    return changed;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::transplant(NodePtr node, NodePtr child) {
    NodePtr parent = node->m_parent;
    if (!parent)
        m_root = child;
    else
        parent->m_pChild[node == parent->m_pChild[1]] = child;
    if (child)
        child->m_parent = parent;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::print(std::ostream& os) const {
    std::lock_guard lock(this->m_mutex);
    print_recursive(os, m_root, "", 2);
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::print_recursive(std::ostream& os, NodePtr node, const std::string& prefix, TX side) const {
    static const char* const branch[] = {"\\-- ", "/-- ", ""};
    if (!node) return;
    print_recursive(os, node->m_pChild[1], prefix + (side == 0 ? "|   " : "    "), 1);
    os << prefix << branch[side] << *node << "\n";
    print_recursive(os, node->m_pChild[0], prefix + (side == 1 ? "|   " : "    "), 0);
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
auto BaseBinaryTree<Traits, Mixins...>::operator=(const BaseBinaryTree& another) -> BaseBinaryTree& {
    if (this == &another)
        return *this;
    std::scoped_lock lock(this->m_mutex, another.m_mutex);
    clear_recursive(m_root);
    m_root = clone_recursive(another.m_root, nullptr);
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::clone_recursive(NodePtr node, NodePtr parent) const -> NodePtr {
    if (!node) return nullptr;
    NodePtr copy = new Node(*node);
    copy->m_parent = parent;
    copy->m_pChild[0] = clone_recursive(node->m_pChild[0], copy);
    copy->m_pChild[1] = clone_recursive(node->m_pChild[1], copy);
    return copy;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::operator=(BaseBinaryTree&& another) noexcept -> BaseBinaryTree& {
    this->move_from(another, [&]{ m_root = std::exchange(another.m_root, nullptr); });
    return *this;
}

#endif // __BINARYTREE_H__
