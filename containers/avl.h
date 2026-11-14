#ifndef __AVL_H__
#define __AVL_H__

#include "binarytree.h"
#include <algorithm>
#include <cstdlib>
#include <ostream>

/** Factor de balance de un nodo AVL: altura del subárbol derecho menos la del izquierdo. */
class Balance {
  signed char m_value = 0;

public:
  /** Registra que el subárbol del lado @p side creció un nivel. */
  void lean(bool side) { m_value += side ? 1 : -1; }
  /** Registra que el subárbol del lado @p side perdió un nivel. */
  void relax(bool side) { m_value -= side ? 1 : -1; }

  /** Indica si ambos subárboles tienen la misma altura. */
  bool balanced() const { return m_value == 0; }
  /** Indica si un subárbol es un nivel más alto que el otro. */
  bool leaning() const { return m_value == 1 || m_value == -1; }
  /** Indica si la diferencia es de dos niveles, lo que obliga a rotar. */
  bool overweight() const { return m_value == 2 || m_value == -2; }
  /** Indica si el subárbol derecho es el más alto. */
  bool right_heavy() const { return m_value > 0; }
  /** Indica si este factor y @p other se inclinan hacia lados contrarios. */
  bool opposes(Balance other) const { return m_value * other.m_value < 0; }

  /** Recalcula los factores tras rotar @p x hacia @p dir, donde @p y es el hijo que subió a su lugar. */
  static void rotated(Balance &x, Balance &y, bool dir) {
    int s = dir ? -1 : 1;
    int bx = s * x.m_value;
    int by = s * y.m_value;
    bx = bx - 1 - std::max(by, 0);
    by = by - 1 + std::min(bx, 0);
    x.m_value = s * bx;
    y.m_value = s * by;
  }

  /** Escribe el factor como entero. */
  friend std::ostream &operator<<(std::ostream &os, Balance b) {
    return os << int(b.m_value);
  }
};

/**
 * Nodo de AVL: nodo de árbol binario con su factor de balance.
 * @tparam T Tipo de la clave.
 * @tparam R Tipo de la referencia.
 */
template <typename T, typename R>
class AVLNode : public BinaryTreeNode<T, AVLNode<T, R>, R> {
public:
  using Parent = BinaryTreeNode<T, AVLNode<T, R>, R>;
  using Parent::Parent;

  /** Factor de balance del nodo. */
  Balance m_balance;
};

/**
 * Árbol AVL: árbol binario de búsqueda que se rebalancea con rotaciones tras cada inserción y eliminación.
 * @tparam Traits Tipos de valor, nodo, comparador e iteradores.
 * @tparam Ops Mixins de recorrido, impresión y lectura.
 */
template <typename Traits, typename Ops = SearchTraversalOps>
class AVLTree : public BinaryTree<Traits, Ops> {
public:
  using Parent = BinaryTree<Traits, Ops>;
  using Parent::height;
  using typename Parent::Node;
  using typename Parent::NodePtr;
  using typename Parent::ref_type;
  using typename Parent::value_type;

  /** Crea un árbol vacío. */
  AVLTree() = default;

  /** Crea un árbol insertando los pares (valor, ref) dados. */
  AVLTree(std::initializer_list<std::pair<value_type, ref_type>> values) {
    fill_from(*this, values);
  }

protected:
  /** Actualiza los factores desde el nodo insertado hacia la raíz y rota donde haga falta. */
  void after_insert(NodePtr node) override {
    for (; node->m_pParent; node = node->m_pParent) {
      NodePtr parent = node->m_pParent;
      parent->m_balance.lean(side_of(node));
      if (parent->m_balance.balanced())
        return;
      if (parent->m_balance.overweight()) {
        redistribute(parent);
        return;
      }
    }
  }

  /** Actualiza los factores desde el punto de eliminación hacia la raíz y rota donde haga falta. */
  void after_remove(NodePtr parent, bool side) override {
    for (; parent; side = side_of(parent), parent = parent->m_pParent) {
      parent->m_balance.relax(side);
      if (parent->m_balance.leaning())
        return;
      if (parent->m_balance.overweight()) {
        parent = redistribute(parent);
        if (!parent->m_balance.balanced())
          return;
      }
    }
  }

  /** Elige el reemplazo en el subárbol más alto (el derecho si están iguales): el sucesor o el predecesor en inorden. */
  NodePtr replacement(NodePtr node) const override {
    bool side = node->m_balance.balanced() || node->m_balance.right_heavy();
    return extreme(node->m_pChild[side], !side);
  }

  /** El reemplazo hereda el factor de balance del nodo eliminado. */
  void take_place(NodePtr repl, const Node *node) override {
    repl->m_balance = node->m_balance;
  }

  /** Altura del subárbol en O(log n): baja siempre por el lado más alto según los factores. */
  std::size_t height(const Node *node) const override {
    std::size_t h = 0;
    for (; node; ++h)
      node = node->m_pChild[node->m_balance.right_heavy()];
    return h;
  }

  /** Escribe el nodo seguido de su factor de balance entre corchetes. */
  void print_node(std::ostream &os, const Node &node) const override {
    os << node << '[' << node.m_balance << ']';
  }

private:
  /** Rota como BinaryTree::rotate y además actualiza los factores de balance. */
  NodePtr rotate(NodePtr x, bool dir) {
    NodePtr y = Parent::rotate(x, dir);
    Balance::rotated(x->m_balance, y->m_balance, dir);
    return y;
  }

  /**
   * Corrige un nodo desbalanceado con una rotación simple o doble.
   * @return La nueva raíz del subárbol.
   */
  NodePtr redistribute(NodePtr x) {
    bool heavy = x->m_balance.right_heavy();
    NodePtr child = x->m_pChild[heavy];
    if (child->m_balance.opposes(x->m_balance))
      rotate(child, heavy);
    return rotate(x, !heavy);
  }
};

#endif // __AVL_H__
