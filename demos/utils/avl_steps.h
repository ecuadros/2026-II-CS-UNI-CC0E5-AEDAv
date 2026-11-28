/** @file
 * Copia sin balancear de un árbol (Unbalanced) y AVLStep, para mostrar las rotaciones del AVL paso a paso.
 */
#pragma once

#include "generic_tests.h"

/** Texto de la clave de un valor. */
template <typename T> std::string KeyText(const T& value) {
    if constexpr (requires { value.key(); })
        return visual::to_text(value.key());
    else
        return visual::to_text(value);
}

/** Tipo de la clave que guardan los nodos de un árbol. */
template <typename Tree>
using TreeKey = std::remove_cvref_t<decltype(std::declval<const Tree&>().PreOrder().begin()->getValue())>;

/**
 * Copia sin balancear de un árbol, para mostrar cómo quedaría antes de las rotaciones del AVL.
 * @tparam Tree Tipo del árbol original.
 */
template <typename Tree>
class Unbalanced : public BinaryTree<BinaryTreeTraits<TreeKey<Tree>, typename Tree::Compare,
                                                       BinaryTreeNode<TreeKey<Tree>, void, typename Tree::ref_type>>> {
    using Parent = BinaryTree<BinaryTreeTraits<TreeKey<Tree>, typename Tree::Compare,
                                               BinaryTreeNode<TreeKey<Tree>, void, typename Tree::ref_type>>>;
    using typename Parent::Node;
    using typename Parent::NodePtr;

    mutable const Node* m_mark = nullptr;

    /** Factor de balance de @p n calculado a partir de las alturas. */
    Int balance(const Node* n) const {
        return static_cast<Int>(this->height(n->m_pChild[1])) - static_cast<Int>(this->height(n->m_pChild[0]));
    }

    /** Elige el reemplazo igual que el AVL, para que ambos árboles eliminen el mismo nodo. */
    NodePtr replacement(NodePtr node) const override {
        Bool side = balance(node) >= 0;
        return extreme(node->m_pChild[side], !side);
    }

    /** Escribe la clave con su factor de balance; el nodo marcado va entre `<` y `>`. */
    void print_node(std::ostream& os, const Node& node) const override {
        Bool marked = &node == m_mark;
        Int bf = balance(&node);
        os << (marked ? "<" : "") << KeyText(node.getValue()) << '[' << (bf > 0 ? "+" : "") << bf << ']'
           << (marked ? ">" : "");
    }

public:
    /** Copia @p tree insertando sus nodos en preorden, lo que conserva su forma. */
    explicit Unbalanced(const Tree& tree) {
        for (auto&& node : tree.PreOrder())
            this->insert(node.getValue(), node.getRef());
    }

    /** Devuelve el primer nodo desbalanceado en postorden, o `nullptr` si no hay ninguno. */
    const Node* unbalanced() const {
        for (auto&& node : this->PostOrder())
            if (std::abs(balance(&node)) > 1) return &node;
        return nullptr;
    }

    /** Explica el caso de desbalance de @p x (LL, RR, LR o RL) y la rotación que lo corrige. */
    std::string describe(const Node* x) const {
        Int bf = balance(x);
        Bool right = bf > 0;
        const Node* c = x->m_pChild[right];
        Bool twice = balance(c) * bf < 0;
        std::string nx = KeyText(x->getValue()), nc = KeyText(c->getValue());
        std::string side = right ? "derecha" : "izquierda";
        std::string other = right ? "izquierda" : "derecha";
        std::ostringstream ss;
        ss << "Nodo " << nx << " desbalanceado (factor " << (bf > 0 ? "+" : "") << bf << "), cargado a la "
           << side << '\n';
        if (twice)
            ss << "Caso " << (right ? "RL" : "LR") << ": rotacion doble -> primero a la " << side << " en " << nc
               << ", luego a la " << other << " en " << nx;
        else
            ss << "Caso " << (right ? "RR" : "LL") << ": rotacion simple a la " << other << " en " << nx;
        return ss.str();
    }

    /** Dibuja el árbol resaltando el nodo @p mark. */
    void print(std::ostream& os, const Node* mark = nullptr) const {
        m_mark = mark;
        this->print_tree(os);
        m_mark = nullptr;
    }
};

/** Aplica @p change al árbol y a su copia sin balancear, y muestra el antes y el después del rebalanceo. */
template <typename Tree, typename Change>
void AVLStep(Tree& tree, const std::string& title, Change change) {
    Unbalanced<Tree> sketch(tree);
    change(tree, sketch);
    PrintColor(title, "1;33");
    if (auto x = sketch.unbalanced()) {
        std::cout << "Como quedaria sin balancear:\n";
        sketch.print(std::cout, x);
        PrintColor(sketch.describe(x), "1;31");
        std::cout << "Luego de rebalancear:\n";
    }
    Unbalanced<Tree>(tree).print(std::cout);
    std::cout << '\n';
}
