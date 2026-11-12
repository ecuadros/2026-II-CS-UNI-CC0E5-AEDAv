#ifndef __MERKLETREE_H__
#define __MERKLETREE_H__

#include "binarytree.h"
#include "traits.h"
#include "traversalops.h"
#include "vector.h"
#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

template <typename T> struct StdMerkleHasher {
  using Digest = std::size_t;

  static Digest mix(Digest seed, Digest v) {
    return seed ^ (v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
  }

  static Digest leaf(const T &data) { return mix(0x00, std::hash<T>{}(data)); }
  static Digest combine(Digest left, Digest right) {
    return mix(mix(0x01, left), right);
  }
};

template <typename Digest>
class MerkleNode : public BinaryTreeNode<Digest, MerkleNode<Digest>> {
public:
  using Parent = BinaryTreeNode<Digest, MerkleNode<Digest>>;
  using Parent::Parent;
};

using MerkleTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp,
                                        RFirstThatOp, PrintableOp, VisualOp>;

template <typename Traits, typename Ops = MerkleTraversalOps>
class MerkleTree : public BinaryTreeBase<Traits, Ops> {
public:
  using Base = BinaryTreeBase<Traits, Ops>;
  using value_type = typename Traits::value_type;
  using Hasher = typename Traits::Hasher;
  using Digest = typename Traits::Digest;
  using Node = typename Traits::Node;
  using NodePtr = Node *;

  struct ProofStep {
    Digest sibling{};
    bool sibling_is_right = false;
  };
  using Proof = Vector<VectorTraits<ProofStep>>;

private:
  using Level = Vector<VectorTraits<NodePtr>>;

  static constexpr bool dup_odd = requires { requires Hasher::duplicate_odd; };

  Level m_leaves;

  static const Digest &second_hash(const Node *parent) {
    return (parent->m_pChild[1] ? parent->m_pChild[1] : parent->m_pChild[0])
        ->getValue();
  }

  static void join(Level &next, NodePtr left, NodePtr right) {
    NodePtr parent = new Node(
        Hasher::combine(left->getValue(),
                        right ? right->getValue() : left->getValue()),
        Ref(-1));
    parent->m_pChild[0] = left;
    parent->m_pChild[1] = right;
    try {
      next.push_back(parent, Ref(next.size()));
    } catch (...) {
      parent->m_pChild[0] = parent->m_pChild[1] = nullptr;
      delete parent;
      throw;
    }
    left->m_pParent = parent;
    if (right)
      right->m_pParent = parent;
  }

  static void drop_internals(NodePtr node) {
    if (!node || node->is_leaf())
      return;
    drop_internals(node->m_pChild[0]);
    drop_internals(node->m_pChild[1]);
    node->m_pChild[0] = node->m_pChild[1] = nullptr;
    delete node;
  }

  static void discard(Level &level, std::size_t from = 0) {
    for (std::size_t i = from; i < level.size(); ++i)
      destroy(level.at(i).getValue());
  }

  void build() {
    Level level;
    try {
      level = m_leaves;
    } catch (...) {
      discard(m_leaves);
      m_leaves.clear();
      throw;
    }
    while (level.size() > 1) {
      Level next;
      std::size_t i = 0;
      try {
        for (; i + 1 < level.size(); i += 2)
          join(next, level.at(i).getValue(), level.at(i + 1).getValue());
        if (i < level.size()) {
          if constexpr (dup_odd)
            join(next, level.at(i).getValue(), nullptr);
          else
            next.push_back(level.at(i).getValue(), Ref(next.size()));
        }
      } catch (...) {
        discard(next);
        discard(level, i);
        m_leaves.clear();
        throw;
      }
      level.swap(next);
    }
    this->m_root = level.empty() ? nullptr : level.at(0).getValue();
  }

  void collect_leaves() {
    m_leaves.clear();
    using Leaves = TreeTraversal<BinaryTreeIterator<Node, TreeOrder::InOrder>>;
    for (auto &node : Leaves(this->m_root))
      if (node.is_leaf())
        m_leaves.push_back(&node, Ref(m_leaves.size()));
  }

protected:
  void reset() override {
    Base::reset();
    m_leaves.clear();
  }

public:
  MerkleTree() = default;

  template <std::input_iterator It>
  MerkleTree(It first, It last) {
    try {
      for (; first != last; ++first) {
        Ref index = Ref(m_leaves.size());
        std::unique_ptr<Node> node(new Node(Hasher::leaf(*first), index));
        m_leaves.push_back(node.get(), index);
        node.release();
      }
    } catch (...) {
      discard(m_leaves);
      throw;
    }
    build();
  }

  MerkleTree(std::initializer_list<value_type> blocks)
      : MerkleTree(blocks.begin(), blocks.end()) {}

  MerkleTree(const MerkleTree &other) : Base(other) { collect_leaves(); }

  MerkleTree(MerkleTree &&other) noexcept { swap(other); }

  MerkleTree &operator=(MerkleTree other) noexcept {
    swap(other);
    return *this;
  }

  void swap(MerkleTree &other) noexcept {
    if (this == &other)
      return;
    std::scoped_lock lock(this->m_mutex, other.m_mutex);
    std::swap(this->m_root, other.m_root);
    m_leaves.swap(other.m_leaves);
  }

  std::size_t size() const override { return m_leaves.size(); }

  Digest root_hash() const {
    std::lock_guard lock(this->m_mutex);
    if (!this->m_root)
      throw std::logic_error("MerkleTree vacio");
    return this->m_root->getValue();
  }

  // shortcut: reconstruye todo el arbol (O(n) hashes); si falla, el arbol queda vacio. Pasar a append incremental (O(log n)) si n es grande.
  void push_back(const value_type &block) {
    std::lock_guard lock(this->m_mutex);
    Ref index = Ref(m_leaves.size());
    std::unique_ptr<Node> node(new Node(Hasher::leaf(block), index));
    m_leaves.push_back(node.get(), index);
    node.release();
    drop_internals(std::exchange(this->m_root, nullptr));
    for (std::size_t i = 0; i < m_leaves.size(); ++i)
      m_leaves.at(i).getValue()->m_pParent = nullptr;
    build();
  }

  void append(const value_type &block) { push_back(block); }

  void update(std::size_t index, const value_type &block) {
    std::lock_guard lock(this->m_mutex);
    NodePtr node = m_leaves.at(index).getValue();
    node->value() = Hasher::leaf(block);
    for (NodePtr p = node->m_pParent; p; p = p->m_pParent)
      p->value() = Hasher::combine(p->m_pChild[0]->getValue(), second_hash(p));
  }

  Proof proof(std::size_t index) const {
    std::lock_guard lock(this->m_mutex);
    Proof steps;
    const Node *node = m_leaves.at(index).getValue();
    for (; node->m_pParent; node = node->m_pParent) {
      bool side = side_of(node);
      const Node *sibling = node->m_pParent->m_pChild[!side];
      steps.push_back({(sibling ? sibling : node)->getValue(), !side},
                      Ref(steps.size()));
    }
    return steps;
  }

  static bool verify(const value_type &block, const Proof &steps,
                     const Digest &root) {
    Digest hash = Hasher::leaf(block);
    for (const auto &step : steps) {
      const ProofStep &s = step.getValue();
      hash = s.sibling_is_right ? Hasher::combine(hash, s.sibling)
                                : Hasher::combine(s.sibling, hash);
    }
    return hash == root;
  }
};

#endif // !__MERKLETREE_H__
