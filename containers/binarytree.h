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

enum class Traversal { PreOrder, InOrder, PostOrder };
enum class Direction { Asc, Desc };
enum Side : TX { Left, Right, Root };

template <typename Node, Traversal Order, Direction Dir>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<Node, Order, Dir>, Node> {
public:
    using MySelf = BinaryTreeIterator<Node, Order, Dir>;
    using Parent = GeneralIterator<MySelf, Node>;
    using NodePtr = Node*;

    explicit BinaryTreeIterator(NodePtr root)
        : Parent(root && Order != Traversal::PreOrder ? next(root, Traversal::PreOrder) : root) {}

    MySelf& operator++() {
        if (Parent::m_ptr)
            Parent::m_ptr = next(Parent::m_ptr, Order);
        return *this;
    }

private:
    static NodePtr next(NodePtr node, Traversal stage);
};

template <typename Node, Traversal Order, Direction Dir>
auto BinaryTreeIterator<Node, Order, Dir>::next(NodePtr node, Traversal stage) -> NodePtr {
    Side first = Dir == Direction::Asc ? Left : Right;
    Side last = Dir == Direction::Asc ? Right : Left;
    do {
        if (stage == Traversal::PostOrder) {
            NodePtr parent = node->m_parent;
            stage = parent && node == parent->m_pChild[first] ? Traversal::InOrder : Traversal::PostOrder;
            node = parent;
        } else if (NodePtr child = node->m_pChild[stage == Traversal::InOrder ? last : first]) {
            node = child;
            stage = Traversal::PreOrder;
        } else {
            stage = stage == Traversal::PreOrder ? Traversal::InOrder : Traversal::PostOrder;
        }
    } while (node && stage != Order);
    return node;
}

template <typename Node>
using BinaryTreeForwardIterator = BinaryTreeIterator<Node, Traversal::InOrder, Direction::Asc>;

template <typename Node>
using BinaryTreeBackwardIterator = BinaryTreeIterator<Node, Traversal::InOrder, Direction::Desc>;

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
    template <Traversal Order, Direction Dir>
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
    void print_recursive(std::ostream& os, NodePtr node, const std::string& prefix, Side side) const;
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

    template <Traversal Order, Direction Dir>
    Iterator<Order, Dir> begin() const { return Iterator<Order, Dir>(m_root); }
    template <Traversal Order, Direction Dir>
    Iterator<Order, Dir> end() const { return Iterator<Order, Dir>(nullptr); }

    template <Traversal Order, Direction Dir, typename Func, typename... Args>
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
    Side branch = Compare()(node->getValue(), value) ? Right : Left;
    return internal_insert(node->m_pChild[branch], value, ref, node); // Tail recursion to insert in the correct branch
}

template <typename Traits, template <typename> typename... Mixins>
bool BaseBinaryTree<Traits, Mixins...>::remove(const value_type& value) {
    std::lock_guard lock(this->m_mutex);
    NodePtr node = m_root;
    while (node) {
        Side branch = Compare()(node->getValue(), value) ? Right : Left;
        if (branch == Left && !Compare()(value, node->getValue()))
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
    NodePtr left = node->m_pChild[Left], right = node->m_pChild[Right];
    NodePtr replacement = left ? left : right;
    NodePtr changed = node->m_parent;
    if (left && right) {
        while (replacement->m_pChild[Right])
            replacement = replacement->m_pChild[Right];
        changed = replacement;
        if (replacement != left) {
            changed = replacement->m_parent;
            transplant(replacement, replacement->m_pChild[Left]);
            replacement->m_pChild[Left] = left;
            left->m_parent = replacement;
        }
        replacement->m_pChild[Right] = right;
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
        parent->m_pChild[node == parent->m_pChild[Right] ? Right : Left] = child;
    if (child)
        child->m_parent = parent;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::print(std::ostream& os) const {
    std::lock_guard lock(this->m_mutex);
    print_recursive(os, m_root, "", Root);
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::print_recursive(std::ostream& os, NodePtr node, const std::string& prefix, Side side) const {
    static const char* const branch[] = {"\\-- ", "/-- ", ""};
    if (!node) return;
    print_recursive(os, node->m_pChild[Right], prefix + (side == Left ? "|   " : "    "), Right);
    os << prefix << branch[side] << *node << "\n";
    print_recursive(os, node->m_pChild[Left], prefix + (side == Right ? "|   " : "    "), Left);
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
    clear_recursive(node->m_pChild[Left]);
    clear_recursive(node->m_pChild[Right]);
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
    copy->m_pChild[Left] = clone_recursive(node->m_pChild[Left], copy);
    copy->m_pChild[Right] = clone_recursive(node->m_pChild[Right], copy);
    return copy;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::operator=(BaseBinaryTree&& another) noexcept -> BaseBinaryTree& {
    this->move_from(another, [&]{ m_root = std::exchange(another.m_root, nullptr); });
    return *this;
}

#endif // __BINARYTREE_H__
