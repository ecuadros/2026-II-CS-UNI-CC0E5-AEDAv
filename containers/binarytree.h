#ifndef __BINARYTREE_H__
#define __BINARYTREE_H__
#include "GeneralNode.h"
#include "GeneralIterator.h"
#include "GeneralContainer.h"
#include "mixins.h"
#include <cstddef>
#include <optional>
#include <utility>

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
    switch (m_TrOrder) {
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
        //NodePtr curr = m_root;
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
    void transplant(NodePtr original, NodePtr sustitute);
    Node remove(value_type val);

    void print() const {
        std::lock_guard lock(this->m_mutex);
        if (!m_root) {
            std::cout << "El árbol está vacío.\n";
            return;
        }

        // 1. Limpiar pantalla y mover el cursor al inicio usando ANSI[cite: 2]
        std::cout << "\033[2J\033[H";

        // --- NIVEL 0: Raíz (Fila 1) ---
        // Ampliamos el espacio horizontal a la columna 25 para dar margen
        std::cout << "\033[1;25H" << m_root->getValue();

        // --- NIVEL 1: Conectores superiores (Fila 3) ---
        std::cout << "\033[3;22H" << "/";
        std::cout << "\033[3;28H" << "\\";

        // --- NIVEL 2: Hijos de la raíz (Fila 5) ---
        if (m_root->m_pChild[0]) {
            std::cout << "\033[5;19H" << m_root->m_pChild[0]->getValue(); // Ej. 31[cite: 2, 3, 4]
        }
        if (m_root->m_pChild[1]) {
            std::cout << "\033[5;31H" << m_root->m_pChild[1]->getValue(); // Ej. 91[cite: 2, 3, 4]
        }

        // --- NIVEL 3: Conectores inferiores (Fila 7) ---
        if (m_root->m_pChild[0]) {
            std::cout << "\033[7;16H" << "/";
            std::cout << "\033[7;22H" << "\\";
        }
        if (m_root->m_pChild[1]) {
            std::cout << "\033[7;28H" << "/";
            std::cout << "\033[7;34H" << "\\";
        }

        // --- NIVEL 4: Hijos de los nodos intermedios / Hojas (Fila 9) ---
        // Hijos del subárbol izquierdo
        if (m_root->m_pChild[0]) {
            auto leftChild = m_root->m_pChild[0];
            if (leftChild->m_pChild[0]) {
                std::cout << "\033[9;13H" << leftChild->m_pChild[0]->getValue(); // Ej. 21[cite: 2, 3, 4]
            }
            if (leftChild->m_pChild[1]) {
                std::cout << "\033[9;21H" << leftChild->m_pChild[1]->getValue(); // Ej. 41[cite: 2, 3, 4]
            }
        }

        // Hijos del subárbol derecho (Aquí se incluye el hijo izquierdo y derecho de 91)
        if (m_root->m_pChild[1]) {
            auto rightChild = m_root->m_pChild[1];
            if (rightChild->m_pChild[0]) {
                std::cout << "\033[9;27H" << rightChild->m_pChild[0]->getValue(); // Hijo izquierdo de 91[cite: 2, 3, 4]
            }
            if (rightChild->m_pChild[1]) {
                std::cout << "\033[9;35H" << rightChild->m_pChild[1]->getValue(); // Hijo derecho de 91 (Ej. 101)[cite: 2, 3, 4]
            }
        }

        // Mover el cursor al final de la impresión para mantener la terminal limpia
        std::cout << "\033[11;1H";
    }
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
    //cout << "AQUI\n";
    while (current) {
        //cout << *current << endl;
        if(current->getValue() == val) return current;
        current = current->m_pChild[current->getValue() < val];
    }
    return current;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseBinaryTree<Traits, Mixins...>::transplant(NodePtr original, NodePtr sustitute) {
    if (!original) m_root = sustitute;
    else {
        auto dir = original == original->m_parent->m_pChild[1];
        original->m_parent->m_pChild[dir] = sustitute;
    }
    if (sustitute && original) sustitute->m_parent = original->m_parent;
    if (original) original->m_parent = nullptr;
}

// template <typename Traits, template <typename> typename... Mixins>
// auto BaseBinaryTree<Traits, Mixins...>::remove(value_type val) -> Node {
//     static Node nullNode;
//     auto nodeFound = findNodePtr(val);
//     if (!nodeFound) return nullNode;
//     auto parent = nodeFound->m_parent;
//     if (!nodeFound.m_pChild[0]) transplant(nodeFound, nodeFound.m_pChild[1]);
//     else if (!nodeFound.m_pChild[1]) transplant(nodeFound, nodeFound.m_pChild[0]);
//     else {
//         auto newNode = extremeLeafNode(nodeFound.m_pChild[0], 1); //Supremo de menores a valor
//         if (nodeFound != newNode->m_parent) {
//             transplant(nodeFound, nodeFound.m_pChild[0]);
//         }
//     }
//     /*auto last = nodeFound->m_pChild[0];
//     if (last) {
//         while (last->m_pChild[1]) last = last->m_pChild[1];
//     }
//     if (last) {
//         parent->m_pChild[parent->m_pChild[1] == nodeFound] = nodeFound->m_pChild[1];
//         nodeFound->m_pChild[1]->m_parent = parent;
//     }
//     else {
//         ;
//         }*/
//     nodeFound->m_parent = nullptr;
//     nodeFound->m_pChild[0] = nullptr;
//     nodeFound->m_pChild[1] = nullptr;
//     return *nodeFound;
// }

template <typename Traits, template <typename> typename... Mixins>
auto BaseBinaryTree<Traits, Mixins...>::remove(value_type val) -> Node {
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

#endif // __BINARYTREE_H__
