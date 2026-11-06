#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralContainer.h"
#include "mixins.h"
#include <cstddef>
#include <optional>
#include <utility>

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>

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
    TX m_height = 0;
};

enum class TransversarOrder {
    PreOrder,
    InOrder,
    PostOrder
};

template <typename T, TX Dir>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<T, Dir>, BinaryTreeNode<T>> {
public:
    using Node = BinaryTreeNode<T>;
    using MySelf = BinaryTreeIterator<T, Dir>;
    using Parent = GeneralIterator<MySelf, Node>;
    using NodePtr = Node*;
    //using Parent::Parent;
    //NodePtr m_parent = nullptr;
private:
    TransversarOrder m_TrOrder;
public:
    BinaryTreeIterator(NodePtr ptr, TransversarOrder _TrOrder=TransversarOrder::InOrder): Parent(ptr), m_TrOrder{_TrOrder} {}
    void setTrOrder(TransversarOrder _TrOrder) { m_TrOrder = _TrOrder; }
    MySelf& operator++();
};

template <typename T, TX Dir>
auto BinaryTreeIterator<T, Dir>::operator++() -> MySelf& {
    auto node = Parent::m_ptr;
    if (node == nullptr)
        return *this;
    if (m_TrOrder == TransversarOrder::InOrder) {
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
    }
    if (m_TrOrder == TransversarOrder::PreOrder) {
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
    }
    else {
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

template <typename T>
using BinaryTreeForwardIterator = BinaryTreeIterator<T, 0>;

template <typename T>
using BinaryTreeBackwardIterator = BinaryTreeIterator<T, 1>;

template <typename T>
struct BinaryTreeTraits {
    using Node              = BinaryTreeNode<T>;
    using ForwardIterator   = BinaryTreeForwardIterator<T>;
    using BackwardIterator  = BinaryTreeBackwardIterator<T>;
    using Order             = TransversarOrder;
};

template <typename T>
struct BinaryTreeAscTraits : public AscendingTraits<T>, public BinaryTreeTraits<T> {};

template <typename T>
struct BinaryTreeDescTraits : public DescendingTraits<T>, public BinaryTreeTraits<T> {};

template <typename Traits, template <typename> typename... Mixins>
class BaseBinaryTree;

template <typename Iterator>
class BinaryTreeProxy;

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
    NodePtr extremeLeafNode(NodePtr nodeptr, TX _Dir) const {
        while (nodeptr != nullptr && nodeptr->m_pChild[_Dir] != nullptr) nodeptr = nodeptr->m_pChild[_Dir];
        return nodeptr;
    }
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

    BinaryTreeProxy<ForwardIterator> InOrder();
    BinaryTreeProxy<BackwardIterator> ReverseInOrder();
    BinaryTreeProxy<ForwardIterator> PreOrder();
    BinaryTreeProxy<BackwardIterator> ReversePreOrder();
    BinaryTreeProxy<ForwardIterator> PostOrder();
    BinaryTreeProxy<BackwardIterator> ReversePostOrder();
    NodePtr findNodePtr(value_type val);
    Node remove(value_type val);
    TX height(NodePtr nodeptr);
    virtual void Redistribution(NodePtr nodeptr) {}

    // Declaración de la función externa como amiga
    template <typename T2, template <typename> typename... Mixins2>
    friend void print_tree(const BaseBinaryTree<T2, Mixins2...>& tree);
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
    node->m_height = height(node);
    Redistribution(node);
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
    return ForwardIterator(extremeLeafNode(m_root, 0));
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::rbegin() const -> BackwardIterator {
    return BackwardIterator(extremeLeafNode(m_root,1));
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

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::InOrder() -> BinaryTreeProxy<ForwardIterator> {
    return BinaryTreeProxy<ForwardIterator>(ForwardIterator(extremeLeafNode(m_root, 0), Order::InOrder), ForwardIterator(nullptr, Order::InOrder));
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::ReverseInOrder() -> BinaryTreeProxy<BackwardIterator> {
    return BinaryTreeProxy<BackwardIterator>(BackwardIterator(extremeLeafNode(m_root, 1), Order::InOrder), BackwardIterator(nullptr, Order::InOrder));
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::PreOrder() -> BinaryTreeProxy<ForwardIterator> {
    return BinaryTreeProxy<ForwardIterator>(ForwardIterator(m_root, Order::PreOrder), ForwardIterator(nullptr, Order::PreOrder));
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::ReversePreOrder() -> BinaryTreeProxy<BackwardIterator> {
    return BinaryTreeProxy<BackwardIterator>(BackwardIterator(m_root, Order::PreOrder), BackwardIterator(nullptr, Order::PreOrder));
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::PostOrder() -> BinaryTreeProxy<ForwardIterator> {
    return BinaryTreeProxy<ForwardIterator>(ForwardIterator(extremeLeafNode(m_root, 0), Order::PostOrder), ForwardIterator(nullptr, Order::PostOrder));
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::ReversePostOrder() -> BinaryTreeProxy<BackwardIterator> {
    return BinaryTreeProxy<BackwardIterator>(BackwardIterator(extremeLeafNode(m_root, 1), Order::PostOrder), BackwardIterator(nullptr, Order::PostOrder));
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::findNodePtr(value_type val) -> NodePtr {
    auto current = m_root;
    while (current) {
        if(current->getValue() == val) return current;
        current = current->m_pChild[current->getValue() < val];
    }
    return current;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::remove(value_type val) -> Node {
    std::lock_guard lock(this->m_mutex);
    static Node nullNode;
    auto nodeFound = findNodePtr(val);
    //cout << *nodeFound << endl;
    if (!nodeFound) return nullNode;
    auto hasLeftChild = nodeFound->m_pChild[0] != nullptr;
    auto hasRightChild = nodeFound->m_pChild[1] != nullptr;
    if (hasRightChild && hasRightChild) {
        auto newNode = extremeLeafNode(nodeFound->m_pChild[0], 1); //Supremo de menores a valor
        std::swap(nodeFound->value(), newNode->value());
        std::swap(nodeFound->ref(), newNode->ref());
        std::swap(nodeFound, newNode);
        hasLeftChild = nodeFound->m_pChild[0] != nullptr;
        hasRightChild = nodeFound->m_pChild[1] != nullptr;
    }
    auto parent = nodeFound->m_parent;
    auto dir = nodeFound == parent->m_pChild[1];
    if (hasLeftChild != hasRightChild) {
        //auto dir = nodeFound == parent->m_pChild[1];
        parent->m_pChild[dir] = nodeFound->m_pChild[hasRightChild];
        parent->m_pChild[dir]->m_parent = parent;
    }
    else {
        parent->m_pChild[dir] = nullptr;
    }
    nodeFound->m_parent = nullptr;
    nodeFound->m_pChild[0] = nullptr;
    nodeFound->m_pChild[1] = nullptr;
    return *nodeFound;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::height(NodePtr nodeptr) -> TX {
    if (!nodeptr) return 0;
    return 1 + std::max(height(nodeptr->m_pChild[0]), height(nodeptr->m_pChild[0]));
}

template <typename Iterator>
class BinaryTreeProxy {
private:
    Iterator m_begin;
    Iterator m_end;
public:
    BinaryTreeProxy(Iterator _begin, Iterator _end): m_begin(_begin), m_end(_end) {}
    Iterator begin() { return m_begin; }
    Iterator end() { return m_end; }
};

// --- FUNCIONES AUXILIARES EXTERNAS ---
template <typename T>
TX tree_get_width(BinaryTreeNode<T>* node) {
    if (!node) return 0;
    return 1 + tree_get_width(node->m_pChild[0]) + tree_get_width(node->m_pChild[1]);
}

template <typename T>
TX tree_get_max_depth(BinaryTreeNode<T>* node) {
    if (!node) return 0;
    return 1 + std::max(tree_get_max_depth(node->m_pChild[0]), tree_get_max_depth(node->m_pChild[1]));
}

template <typename T>
void tree_draw_orthogonal(BinaryTreeNode<T>* node, TX x, TX y, std::vector<std::string>& canvas) {
    if (!node) return;

    TX left_w = tree_get_width(node->m_pChild[0]);
    TX node_x = x + left_w * 4;

    std::ostringstream oss;
    oss << node->getValue();
    std::string val = oss.str();
    TX start_x = node_x - val.length() / 2;

    for (TX i = 0; i < static_cast<TX>(val.length()); i++) {
        if (start_x + i >= 0 && start_x + i < static_cast<TX>(canvas[y].size())) {
            canvas[y][start_x + i] = val[i];
        }
    }

    TX mid_y = y + 1;

    // Conexión al hijo izquierdo
    if (node->m_pChild[0]) {
        TX left_child_x = x + tree_get_width(node->m_pChild[0]->m_pChild[0]) * 4;
        canvas[mid_y][node_x] = '|';

        TX min_x = std::min(node_x, left_child_x);
        TX max_x = std::max(node_x, left_child_x);
        for (TX i = min_x; i <= max_x; i++) canvas[mid_y][i] = '-';

        canvas[mid_y][node_x] = '+';
        canvas[mid_y][left_child_x] = '+';
        canvas[mid_y + 1][left_child_x] = '|';
        tree_draw_orthogonal(node->m_pChild[0], x, y + 2, canvas);
    }

    // Conexión al hijo derecho
    if (node->m_pChild[1]) {
        TX right_child_x = x + (left_w + 1 + tree_get_width(node->m_pChild[1]->m_pChild[0])) * 4;
        canvas[mid_y][node_x] = '|';

        TX min_x = std::min(node_x, right_child_x);
        TX max_x = std::max(node_x, right_child_x);
        for (TX i = min_x; i <= max_x; i++) canvas[mid_y][i] = '-';

        canvas[mid_y][node_x] = '+';
        canvas[mid_y][right_child_x] = '+';
        canvas[mid_y + 1][right_child_x] = '|';
        tree_draw_orthogonal(node->m_pChild[1], x + (left_w + 1) * 4, y + 2, canvas);
    }
}

// --- FUNCIÓN PRINCIPAL EXTERNA ---
template <typename Traits, template <typename> typename... Mixins>
void print_tree(const BaseBinaryTree<Traits, Mixins...>& tree) {
    std::lock_guard lock(tree.m_mutex);
    auto root = tree.m_root;
    if (!root) {
        std::cout << "El árbol está vacío.\n";
        return;
    }

    // 2. Calcular dimensiones
    TX tree_width = tree_get_width(root);
    TX tree_height = tree_get_max_depth(root);

    // Ajustamos un poco el alto. 2 filas por nivel es suficiente.
    TX canvas_width = tree_width * 4 + 8;
    TX canvas_height = tree_height * 2;

    // 3. Crear lienzo
    std::vector<std::string> canvas(canvas_height, std::string(canvas_width, ' '));

    // 4. Dibujar
    tree_draw_orthogonal(root, 4, 0, canvas);

    // 5. Imprimir
    for (const auto& row : canvas) {
        size_t last = row.find_last_not_of(' ');
        if (last != std::string::npos) {
            std::cout << row.substr(0, last + 1) << "\n";
        } else {
            std::cout << "\n";
        }
    }
}

#endif // __BINARYTREE_H__
