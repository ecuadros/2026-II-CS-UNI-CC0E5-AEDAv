#ifndef __AVL_H__
#define __AVL_H__

#include "binarytree.h"
#include <algorithm>
#include <cstdlib>
#include <ostream>

class Balance {
  signed char m_value = 0;

public:
  void lean(bool side) { m_value += side ? 1 : -1; }
  void relax(bool side) { m_value -= side ? 1 : -1; }

  bool balanced() const { return m_value == 0; }
  bool leaning() const { return m_value == 1 || m_value == -1; }
  bool overweight() const { return m_value == 2 || m_value == -2; }
  bool right_heavy() const { return m_value > 0; }
  bool opposes(Balance other) const { return m_value * other.m_value < 0; }

  static void rotated(Balance &x, Balance &y, bool dir) {
    int s = dir ? -1 : 1;
    int bx = s * x.m_value;
    int by = s * y.m_value;
    bx = bx - 1 - std::max(by, 0);
    by = by - 1 + std::min(bx, 0);
    x.m_value = s * bx;
    y.m_value = s * by;
  }

  friend std::ostream &operator<<(std::ostream &os, Balance b) {
    return os << int(b.m_value);
  }
};

template <typename T, typename R>
class AVLNode : public BinaryTreeNode<T, AVLNode<T, R>, R> {
public:
  using Parent = BinaryTreeNode<T, AVLNode<T, R>, R>;
  using Parent::Parent;

  Balance m_balance;
};

template <typename Traits, typename Ops = SearchTraversalOps>
class AVLTree : public BinaryTree<Traits, Ops> {
public:
  using Parent = BinaryTree<Traits, Ops>;
  using Parent::height;
  using typename Parent::Node;
  using typename Parent::NodePtr;
  using typename Parent::ref_type;
  using typename Parent::value_type;

  AVLTree() = default;

  AVLTree(std::initializer_list<std::pair<value_type, ref_type>> values) {
    fill_from(*this, values);
  }

protected:
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

  NodePtr replacement(NodePtr node) const override {
    bool side = node->m_balance.balanced() || node->m_balance.right_heavy();
    return extreme(node->m_pChild[side], !side);
  }

  void take_place(NodePtr repl, const Node *node) override {
    repl->m_balance = node->m_balance;
  }

  std::size_t height(const Node *node) const override {
    std::size_t h = 0;
    for (; node; ++h)
      node = node->m_pChild[node->m_balance.right_heavy()];
    return h;
  }

  void print_node(std::ostream &os, const Node &node) const override {
    os << node << '[' << node.m_balance << ']';
  }

private:
  NodePtr rotate(NodePtr x, bool dir) {
    NodePtr y = Parent::rotate(x, dir);
    Balance::rotated(x->m_balance, y->m_balance, dir);
    return y;
  }

  NodePtr redistribute(NodePtr x) {
    bool heavy = x->m_balance.right_heavy();
    NodePtr child = x->m_pChild[heavy];
    if (child->m_balance.opposes(x->m_balance))
      rotate(child, heavy);
    return rotate(x, !heavy);
  }
};

#endif // __AVL_H__
