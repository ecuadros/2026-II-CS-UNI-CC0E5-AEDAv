#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralContainer.h"
#include "mixins.h"
#include "../utilities/printcolored.h"
#include <sstream>
#include <string>

template <typename T, typename Derived>
class BaseBinaryNode : public GeneralNode<T> {
public:
    using value_type        = T;
    using MySelf            = Derived;
    using Node              = Derived;
    using NodePtr           = Node*;
    using Parent            = GeneralNode<T>;
    using Parent::Parent; // Inherit constructor

    NodePtr m_pChild[2] = {nullptr, nullptr}; // left and right children
    NodePtr m_parent = nullptr;

};

template <typename T>
class BinaryTreeNode : public BaseBinaryNode<T, BinaryTreeNode<T>> {
public:
    using Parent = BaseBinaryNode<T, BinaryTreeNode<T>>;
    using Parent::Parent;
};

enum TraversalOrder : TX { InOrder = 0, PreOrder = 1, PostOrder = 2 };
enum TraversalDir    : TX { Asc = 0, Desc = 1 };

template <typename NodeT, TX Order = InOrder, TX Dir = Asc>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<NodeT, Order, Dir>, NodeT> {
public:
    using Node = NodeT;
    using MySelf = BinaryTreeIterator<NodeT, Order, Dir>;
    using Parent = GeneralIterator<MySelf, Node>;
    using NodePtr = Node*;
    using Parent::Parent;
    NodePtr m_parent = nullptr;

    NodePtr GetNode() const { return Parent::m_ptr; }

    MySelf& operator++();
};

template <typename NodeT, TX Order, TX Dir>
auto BinaryTreeIterator<NodeT, Order, Dir>::operator++() -> MySelf& {
    constexpr int First = Dir, Second = 1 - Dir; // orden de visita de hijos

    NodePtr n = Parent::m_ptr;
    if (!n) return *this;

    // primer hijo en el recorrido
    auto firstChild = [](NodePtr x) { return x->m_pChild[First] ? x->m_pChild[First] : x->m_pChild[Second]; };

    // hoja siguiendo el primer hijo
    auto firstLeaf = [&](NodePtr x) { while (NodePtr c = firstChild(x)) x = c; return x; };

    // hermano Second del ancestro donde x es hijo First
    auto nextSibling = [](NodePtr x) {
        NodePtr p = x->m_parent;
        while (p && (x != p->m_pChild[First] || !p->m_pChild[Second]))
            x = p, p = p->m_parent;
        return p ? p->m_pChild[Second] : nullptr;
    };

    switch (Order) {
        case InOrder:
            if (NodePtr r = n->m_pChild[Second]) {
                while (r->m_pChild[First]) r = r->m_pChild[First];
                n = r;
            } else {
                NodePtr p = n->m_parent;
                for (; p && n == p->m_pChild[Second]; n = p, p = p->m_parent);
                n = p;
            }
            break;
        case PreOrder:
            n = firstChild(n) ? firstChild(n) : nextSibling(n);
            break;
        default: { // PostOrder
            NodePtr p = n->m_parent, sib = p ? p->m_pChild[Second] : nullptr;
            n = (sib && n != sib) ? firstLeaf(sib) : p;
            break;
        }
    }

    Parent::m_ptr = n;
    return *this;
}

template <typename NodeT>
using BinaryTreeForwardIterator = BinaryTreeIterator<NodeT, InOrder, Asc>;

template <typename NodeT>
using BinaryTreeBackwardIterator = BinaryTreeIterator<NodeT, InOrder, Desc>;

template <typename T>
struct BinaryTreeTraits {
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeIterator<Node, InOrder, Asc>;
    using BackwardIterator  = BinaryTreeIterator<Node, InOrder, Desc>;
};

template <typename T>
struct BinaryTreeAscTraits : public AscendingTraits<T>, public BinaryTreeTraits<T> {};

template <typename T>
struct BinaryTreeDescTraits : public DescendingTraits<T>, public BinaryTreeTraits<T> {};

template <typename Node, typename Compare>
void BinaryTreeInsert(typename Node::NodePtr& node, const typename Node::value_type& value, Ref ref, Node* parent, Node** leaf_out = nullptr) {
    if (!node) {
        node = new Node(value, ref);
        node->m_parent = parent;
        if (leaf_out)
            *leaf_out = node;
        return;
    }
    auto branch = Compare()(node->getValue(), value);
    BinaryTreeInsert<Node, Compare>(node->m_pChild[branch], value, ref, node, leaf_out);
}

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

protected:
    using Parent::m_mutex;
    NodePtr m_root = nullptr; // root of the binary tree

    virtual void PrintNode(std::ostream &os, NodePtr node, Color color) const {
        PrintColored(os, *node, color, false);
    }

    // ancho visible del label
    std::size_t LabelWidth(NodePtr node) const {
        std::ostringstream label;
        PrintNode(label, node, Color::White);
        return label.str().size();
    }

    // sube a la raiz; al volver emite el riel de cada ancestro
    std::size_t PrintPrefix(std::ostream &os, NodePtr node, bool rightIn, bool bendIn, bool direct, std::size_t &col) const {
        std::size_t c;
        if (node->m_parent) {
            bool right = node == node->m_parent->m_pChild[1];
            c = PrintPrefix(os, node->m_parent, right, rightIn != right, false, col) + 4 + LabelWidth(node) + 1;
        } else {
            c = LabelWidth(node) + 1;
        }
        if (!direct) {
            os << std::string(c - col, ' ') << (bendIn ? "│" : " ");
            col = c + 1;
        }
        return c;
    }

    // linea de un nodo via el camino a la raiz
    void PrintLine(std::ostream &os, NodePtr node) const {
        static const Color kLevelColors[] = {Color::Red, Color::Green, Color::Yellow, Color::Blue, Color::Magenta, Color::Cyan}; // colores por nivel
        TX depth = 0;
        for (NodePtr p = node->m_parent; p; p = p->m_parent) ++depth;
        size_t col = 0;
        string connector = "";
        if (node->m_parent) {
            bool right = node == node->m_parent->m_pChild[1];
            std::size_t pcol = PrintPrefix(os, node->m_parent, right, false, true, col);
            os << std::string(pcol - col, ' ');
            connector = right ? "┌── " : "└── ";
        }
        // label del nodo
        std::ostringstream label;
        PrintNode(label, node, kLevelColors[depth % 6]);
        os << connector;
        PrintColored(os, label.str(), kLevelColors[depth % 6], false);
        if (node->m_pChild[0] || node->m_pChild[1])
            os << " ┤";
        os << "\n";
    }

    NodePtr detach_min(NodePtr sub, NodePtr subParent) {
        while (sub->m_pChild[0]) {
            subParent = sub;
            sub = sub->m_pChild[0];
        }
        if (subParent->m_pChild[0] == sub)
            subParent->m_pChild[0] = sub->m_pChild[1];
        else
            subParent->m_pChild[1] = sub->m_pChild[1];
        if (sub->m_pChild[1])
            sub->m_pChild[1]->m_parent = subParent;
        return sub;
    }
    void internal_remove(NodePtr& node, NodePtr parent, const value_type& value, NodePtr* out, NodePtr* lowest) {
        if (!node)
            return;
        if (Compare()(value, node->getValue()))
            return internal_remove(node->m_pChild[0], node, value, out, lowest);
        if (Compare()(node->getValue(), value))
            return internal_remove(node->m_pChild[1], node, value, out, lowest);
        *out = node;
        if (!node->m_pChild[0] || !node->m_pChild[1]) {
            node = node->m_pChild[node->m_pChild[0] ? 0 : 1];
            *lowest = parent;
        } else {
            NodePtr succ = detach_min(node->m_pChild[1], node);
            *lowest = (succ->m_parent == node) ? succ : succ->m_parent;
            succ->m_pChild[0] = node->m_pChild[0];
            succ->m_pChild[1] = node->m_pChild[1];
            if (succ->m_pChild[0])
                succ->m_pChild[0]->m_parent = succ;
            if (succ->m_pChild[1])
                succ->m_pChild[1]->m_parent = succ;
            succ->m_parent = parent;
            node = succ;
            return;
        }
        if (node)
            node->m_parent = parent;
    }
public:
    BaseBinaryTree() = default;
    BaseBinaryTree(const BaseBinaryTree& another) { *this = another; }
    BaseBinaryTree(BaseBinaryTree&& another) noexcept { *this = std::move(another); }
    virtual ~BaseBinaryTree() { clear(); }

    virtual void insert(const value_type& value, Ref ref) {
        std::lock_guard lock(this->m_mutex);
        BinaryTreeInsert<Node, Compare>(m_root, value, ref, nullptr);
    }
    // Move constructor and move assignment operator
    // Constructor and destructor
    // Persistencia (operator << y operator >> globales para todas las estructuras pero si es para BT con indentacion entonces hacerlo por separado)
    // Concurrencia
    // Iteradores (forward y backward)
    // FirstThat, ForEach, call, rcall por herencia

    void clear() override;

    void push_back(const value_type& value, Ref ref) override { insert(value, ref); }

    virtual Node remove(const value_type& value) {
        std::lock_guard lock(this->m_mutex);
        NodePtr removed = nullptr, lowest = nullptr;
        internal_remove(m_root, nullptr, value, &removed, &lowest);
        if (!removed)
            return Node();
        Node result(removed->getValue(), removed->getRef());
        delete removed;
        return result;
    }

    void clear_recursive(NodePtr node);

    ForwardIterator begin() const override;
    ForwardIterator end() const override { return ForwardIterator(nullptr); }

    BackwardIterator rbegin() const;
    BackwardIterator rend() const { return BackwardIterator(nullptr); }

    template <TX Order, TX Dir>
    NodePtr first_node(NodePtr node) const {
        if (node && Order != PreOrder) {
            if (Order == InOrder) {
                while (node->m_pChild[Dir])
                    node = node->m_pChild[Dir];
            } else {
                while (node->m_pChild[Dir] || node->m_pChild[1-Dir])
                    node = node->m_pChild[node->m_pChild[Dir] ? Dir : 1-Dir];
            }
        }
        return node;
    }

    template <TX Order, TX Dir = Asc>
    BinaryTreeIterator<Node, Order, Dir> begin() const {
        return BinaryTreeIterator<Node, Order, Dir>(first_node<Order, Dir>(m_root));
    }

    template <TX Order, TX Dir = Asc>
    BinaryTreeIterator<Node, Order, Dir> end() const {
        return BinaryTreeIterator<Node, Order, Dir>(nullptr);
    }

    NodePtr GetRoot() const { return m_root; }

    virtual void PrintTree(std::ostream &os = std::cout) const {
        // lineas en inorden desc
        for (auto it = begin<InOrder, Desc>(); it != end<InOrder, Desc>(); ++it)
            PrintLine(os, it.GetNode());
        os << "\n\n";
    }

    BaseBinaryTree& operator=(const BaseBinaryTree& another);
    BaseBinaryTree& operator=(BaseBinaryTree&& another) noexcept;
};

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

#endif // __BINARYTREE_H__