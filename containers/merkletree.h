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

/**
 * Hasher de Merkle basado en `std::hash`. No es criptográfico: sirve para demos y pruebas.
 * @tparam T Tipo de los bloques de datos.
 */
template <typename T> struct StdMerkleHasher {
  using Digest = std::size_t;

  /** Mezcla @p v en @p seed, al estilo de `boost::hash_combine`. */
  static Digest mix(Digest seed, Digest v) {
    return seed ^ (v + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
  }

  /** Hash de una hoja; lleva el prefijo 0x00 para distinguirla de un nodo interno. */
  static Digest leaf(const T &data) { return mix(0x00, std::hash<T>{}(data)); }
  /** Hash de un nodo interno a partir de sus dos hijos; lleva el prefijo 0x01. */
  static Digest combine(Digest left, Digest right) {
    return mix(mix(0x01, left), right);
  }
};

/**
 * Nodo de un árbol de Merkle: su valor es el hash. En las hojas la referencia es el índice del bloque; en los nodos internos es -1.
 * @tparam Digest Tipo del hash.
 */
template <typename Digest>
class MerkleNode : public BinaryTreeNode<Digest, MerkleNode<Digest>> {
public:
  using Parent = BinaryTreeNode<Digest, MerkleNode<Digest>>;
  using Parent::Parent;
};

/** Operaciones del MerkleTree: recorridos, impresión y dibujo; sin lectura desde stream. */
using MerkleTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp,
                                        RFirstThatOp, PrintableOp, VisualOp>;

/**
 * Árbol de Merkle: las hojas guardan el hash de cada bloque y cada nodo interno el hash de sus dos hijos, de modo que la raíz resume todos los datos.
 * @tparam Traits Tipos de bloque, hasher, digest y nodo.
 * @tparam Ops Mixins de recorrido e impresión.
 */
template <typename Traits, typename Ops = MerkleTraversalOps>
class MerkleTree : public BinaryTreeBase<Traits, Ops> {
public:
  using Base = BinaryTreeBase<Traits, Ops>;
  using value_type = typename Traits::value_type;
  using Hasher = typename Traits::Hasher;
  using Digest = typename Traits::Digest;
  using Node = typename Traits::Node;
  using NodePtr = Node *;

  /** Paso de una prueba de inclusión: el hash hermano y de qué lado va al combinar. */
  struct ProofStep {
    Digest sibling{};
    bool sibling_is_right = false;
  };
  /** Prueba de inclusión: los pasos desde la hoja hasta la raíz. */
  using Proof = Vector<VectorTraits<ProofStep>>;

private:
  using Level = Vector<VectorTraits<NodePtr>>;

  static constexpr bool dup_odd = requires { requires Hasher::duplicate_odd; };

  /** Hojas en el orden de los bloques. */
  Level m_leaves;

  /** Hash del segundo operando de @p parent: el hijo derecho, o el izquierdo si el nodo impar se duplicó. */
  static const Digest &second_hash(const Node *parent) {
    return (parent->m_pChild[1] ? parent->m_pChild[1] : parent->m_pChild[0])
        ->getValue();
  }

  /** Crea un nodo interno con hijos @p left y @p right sin tocar los padres de los hijos. Si @p right es nulo, combina @p left consigo mismo. */
  static NodePtr make_parent(NodePtr left, NodePtr right) {
    NodePtr parent = new Node(
        Hasher::combine(left->getValue(),
                        right ? right->getValue() : left->getValue()),
        Ref(-1));
    parent->m_pChild[0] = left;
    parent->m_pChild[1] = right;
    return parent;
  }

  /** Crea el padre de @p left y @p right y lo añade al nivel @p next. Si @p right es nulo, combina @p left consigo mismo. */
  static void join(Level &next, NodePtr left, NodePtr right) {
    NodePtr parent = make_parent(left, right);
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

  /** Hijo por el que continúa la columna derecha: el derecho, o el izquierdo si el nodo duplicó al impar. */
  static NodePtr spine_child(const Node *node) {
    return node->m_pChild[1] ? node->m_pChild[1] : node->m_pChild[0];
  }

  /** Libera los nodos internos desde @p top bajando por la columna derecha hasta @p stop (sin incluirlo). Los hijos no se liberan. */
  static void release_spine(NodePtr top, const Node *stop) {
    while (top != stop) {
      NodePtr next = spine_child(top);
      top->m_pChild[0] = top->m_pChild[1] = nullptr;
      delete top;
      top = next;
    }
  }

  /** Enlaza el padre de los hijos de cada nodo de la columna derecha, desde @p top hasta @p stop (sin incluirlo). */
  static void adopt_spine(NodePtr top, const Node *stop) {
    for (NodePtr node = top; node != stop; node = spine_child(node))
      for (NodePtr child : node->m_pChild)
        if (child)
          child->m_pParent = node;
  }

  /** Libera los subárboles del nivel @p level a partir de la posición @p from. */
  static void discard(Level &level, std::size_t from = 0) {
    for (std::size_t i = from; i < level.size(); ++i)
      destroy(level.at(i).getValue());
  }

  /** Construye los niveles internos desde las hojas hasta la raíz. Un nodo sin pareja sube tal cual al nivel siguiente, o se combina consigo mismo si el hasher define `duplicate_odd`. Si falla, libera todo y el árbol queda vacío. */
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

  /** Reconstruye la lista de hojas recorriendo el árbol de izquierda a derecha. */
  void collect_leaves() {
    m_leaves.clear();
    using Leaves = TreeTraversal<BinaryTreeIterator<Node, TreeOrder::InOrder>>;
    for (auto &node : Leaves(this->m_root))
      if (node.is_leaf())
        m_leaves.push_back(&node, Ref(m_leaves.size()));
  }

protected:
  /** Libera los nodos y vacía la lista de hojas. */
  void reset() override {
    Base::reset();
    m_leaves.clear();
  }

public:
  /** Crea un árbol vacío. */
  MerkleTree() = default;

  /** Construye el árbol con los bloques del rango [first, last). */
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

  /** Construye el árbol con los bloques dados. */
  MerkleTree(std::initializer_list<value_type> blocks)
      : MerkleTree(blocks.begin(), blocks.end()) {}

  /** Copia en profundidad @p other. */
  MerkleTree(const MerkleTree &other) : Base(other) { collect_leaves(); }

  /** Mueve el contenido de @p other, que queda vacío. */
  MerkleTree(MerkleTree &&other) noexcept { swap(other); }

  /** Asigna por copia o por movimiento (copy-and-swap). */
  MerkleTree &operator=(MerkleTree other) noexcept {
    swap(other);
    return *this;
  }

  /** Intercambia el contenido con @p other. */
  void swap(MerkleTree &other) noexcept {
    if (this == &other)
      return;
    std::scoped_lock lock(this->m_mutex, other.m_mutex);
    std::swap(this->m_root, other.m_root);
    m_leaves.swap(other.m_leaves);
  }

  /** Cantidad de bloques (hojas). */
  std::size_t size() const override { return m_leaves.size(); }

  /**
   * Devuelve el hash raíz.
   * @throws std::logic_error si el árbol está vacío.
   */
  Digest root_hash() const {
    std::lock_guard lock(this->m_mutex);
    if (!this->m_root)
      throw std::logic_error("MerkleTree vacio");
    return this->m_root->getValue();
  }

  /**
   * Añade un bloque al final. Las hojas se agrupan en subárboles perfectos según los bits de n (los "picos");
   * solo se rehace la columna derecha que los une, así que cuesta O(log n) hashes. Si falla, el árbol no cambia.
   */
  void push_back(const value_type &block) {
    std::lock_guard lock(this->m_mutex);
    constexpr std::size_t MaxPeaks = sizeof(std::size_t) * 8;
    const std::size_t n = m_leaves.size();
    Ref index = Ref(n);
    std::unique_ptr<Node> leaf(new Node(Hasher::leaf(block), index));

    NodePtr peak[MaxPeaks];
    std::size_t height[MaxPeaks];
    std::size_t peaks = 0;
    for (std::size_t bit = MaxPeaks, start = 0; bit-- > 0;)
      if ((n >> bit) & 1) {
        NodePtr node = m_leaves.at(start).getValue();
        for (std::size_t i = 0; i < bit; ++i)
          node = node->m_pParent;
        peak[peaks] = node;
        height[peaks++] = bit;
        start += std::size_t(1) << bit;
      }

    NodePtr top = leaf.get();
    [[maybe_unused]] std::size_t top_height = 0;
    try {
      for (std::size_t i = peaks; i-- > 0;) {
        if constexpr (dup_odd)
          for (; top_height < height[i]; ++top_height)
            top = make_parent(top, nullptr);
        top = make_parent(peak[i], top);
        top_height = height[i] + 1;
      }
      m_leaves.push_back(leaf.get(), index);
    } catch (...) {
      release_spine(top, leaf.get());
      throw;
    }
    release_spine(std::exchange(this->m_root, top), peaks ? peak[peaks - 1] : nullptr);
    top->m_pParent = nullptr;
    adopt_spine(top, leaf.get());
    leaf.release();
  }

  /** Equivale a push_back(). */
  void append(const value_type &block) { push_back(block); }

  /**
   * Reemplaza el bloque @p index y recalcula solo los hashes de su camino hasta la raíz.
   * @throws std::out_of_range si @p index no existe.
   */
  void update(std::size_t index, const value_type &block) {
    std::lock_guard lock(this->m_mutex);
    NodePtr node = m_leaves.at(index).getValue();
    node->value() = Hasher::leaf(block);
    for (NodePtr p = node->m_pParent; p; p = p->m_pParent)
      p->value() = Hasher::combine(p->m_pChild[0]->getValue(), second_hash(p));
  }

  /**
   * Genera la prueba de inclusión del bloque @p index.
   * @throws std::out_of_range si @p index no existe.
   */
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

  /** Comprueba que @p block pertenece al árbol cuyo hash raíz es @p root, usando solo la prueba @p steps. */
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
