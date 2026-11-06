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
#include <mutex>
#include <ostream>
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

  bool is_leaf() const { return !this->m_pChild[0] && !this->m_pChild[1]; }
};

using MerkleTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp,
                                        RFirstThatOp, PrintableOp, VisualOp>;

template <typename Traits, typename Ops = MerkleTraversalOps>
class MerkleTree : public Ops {
public:
  using value_type = typename Traits::value_type;
  using Hasher = typename Traits::Hasher;
  using Digest = typename Traits::Digest;
  using Node = typename Traits::Node;
  using NodePtr = Node *;
  using ForwardIterator = typename Traits::ForwardIterator;
  using BackwardIterator = typename Traits::BackwardIterator;

  struct ProofStep {
    Digest sibling{};
    bool sibling_is_right = false;
  };
  using Proof = Vector<VectorTraits<ProofStep>>;

private:
  using Level = Vector<VectorTraits<NodePtr>>;

  template <TreeOrder Order, bool Reverse = false, typename N = const Node>
  using Traversal = TreeTraversal<BinaryTreeIterator<N, Order, Reverse>>;

  NodePtr m_root = nullptr;
  Level m_leaves;

  static void discard(Level &level, std::size_t from = 0) {
    for (std::size_t i = from; i < level.size(); ++i)
      destroy(level.at(i).getValue());
  }

  void build() {
    Level level = m_leaves;
    while (level.size() > 1) {
      Level next;
      std::size_t i = 0;
      try {
        for (; i + 1 < level.size(); i += 2) {
          NodePtr left = level.at(i).getValue();
          NodePtr right = level.at(i + 1).getValue();
          NodePtr parent = new Node(
              Hasher::combine(left->getValue(), right->getValue()), Ref(-1));
          parent->m_pChild[0] = left;
          parent->m_pChild[1] = right;
          try {
            next.push_back(parent, Ref(next.size()));
          } catch (...) {
            parent->m_pChild[0] = parent->m_pChild[1] = nullptr;
            delete parent;
            throw;
          }
          left->m_pParent = right->m_pParent = parent;
        }
        if (i < level.size())
          next.push_back(level.at(i).getValue(), Ref(next.size()));
      } catch (...) {
        discard(next);
        discard(level, i);
        m_leaves.clear();
        throw;
      }
      level.swap(next);
    }
    m_root = level.empty() ? nullptr : level.at(0).getValue();
  }

  void collect_leaves() {
    m_leaves.clear();
    for (auto &node : Traversal<TreeOrder::InOrder, false, Node>(m_root))
      if (node.is_leaf())
        m_leaves.push_back(&node, Ref(m_leaves.size()));
  }

public:
  MerkleTree() = default;

  template <std::input_iterator It>
  MerkleTree(It first, It last) {
    try {
      for (; first != last; ++first) {
        Ref index = Ref(m_leaves.size());
        m_leaves.push_back(new Node(Hasher::leaf(*first), index), index);
      }
    } catch (...) {
      discard(m_leaves);
      throw;
    }
    build();
  }

  MerkleTree(std::initializer_list<value_type> blocks)
      : MerkleTree(blocks.begin(), blocks.end()) {}

  MerkleTree(const MerkleTree &other) {
    std::lock_guard lock(other.m_mutex);
    m_root = clone(other.m_root);
    collect_leaves();
  }

  MerkleTree(MerkleTree &&other) noexcept { swap(other); }

  MerkleTree &operator=(MerkleTree other) noexcept {
    swap(other);
    return *this;
  }

  void swap(MerkleTree &other) noexcept {
    if (this == &other)
      return;
    std::scoped_lock lock(this->m_mutex, other.m_mutex);
    std::swap(m_root, other.m_root);
    m_leaves.swap(other.m_leaves);
  }

  ~MerkleTree() { clear(); }

  void clear() {
    std::lock_guard lock(this->m_mutex);
    destroy(std::exchange(m_root, nullptr));
    m_leaves.clear();
  }

  std::size_t size() const { return m_leaves.size(); }
  bool empty() const { return size() == 0; }

  Digest root_hash() const {
    std::lock_guard lock(this->m_mutex);
    if (!m_root)
      throw std::logic_error("MerkleTree vacio");
    return m_root->getValue();
  }

  void update(std::size_t index, const value_type &block) {
    std::lock_guard lock(this->m_mutex);
    NodePtr node = m_leaves.at(index).getValue();
    node->value() = Hasher::leaf(block);
    for (NodePtr p = node->m_pParent; p; p = p->m_pParent)
      p->value() = Hasher::combine(p->m_pChild[0]->getValue(),
                                   p->m_pChild[1]->getValue());
  }

  Proof proof(std::size_t index) const {
    std::lock_guard lock(this->m_mutex);
    Proof steps;
    const Node *node = m_leaves.at(index).getValue();
    for (; node->m_pParent; node = node->m_pParent) {
      bool side = side_of(node);
      steps.push_back({node->m_pParent->m_pChild[!side]->getValue(), !side},
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

  struct View {
    using Handle = const Node *;
    const MerkleTree *tree;

    Handle root() const { return tree->m_root; }
    Handle child(Handle node, bool side) const { return node->m_pChild[side]; }
    bool valid(Handle node) const { return node != nullptr; }
    void label(std::ostream &os, Handle node) const { os << *node; }
    std::size_t height(Handle node) const {
      if (!node)
        return 0;
      return 1 + std::max(height(node->m_pChild[0]), height(node->m_pChild[1]));
    }
  };

  View tree_view() const { return View{this}; }

  ForwardIterator begin() const { return ForwardIterator(m_root); }
  ForwardIterator end() const { return ForwardIterator(nullptr); }
  BackwardIterator rbegin() const { return BackwardIterator(m_root); }
  BackwardIterator rend() const { return BackwardIterator(nullptr); }

  std::size_t height() const {
    std::lock_guard lock(this->m_mutex);
    return View{this}.height(m_root);
  }

  auto InOrder() const { return Traversal<TreeOrder::InOrder>(m_root); }
  auto ReverseInOrder() const { return Traversal<TreeOrder::InOrder, true>(m_root); }
  auto PreOrder() const { return Traversal<TreeOrder::PreOrder>(m_root); }
  auto ReversePreOrder() const { return Traversal<TreeOrder::PostOrder, true>(m_root); }
  auto PostOrder() const { return Traversal<TreeOrder::PostOrder>(m_root); }
  auto ReversePostOrder() const { return Traversal<TreeOrder::PreOrder, true>(m_root); }
};

template <typename T, typename Hasher = StdMerkleHasher<T>>
using MerkleOf = MerkleTree<MerkleTreeTraits<T, Hasher>>;

#endif // !__MERKLETREE_H__
