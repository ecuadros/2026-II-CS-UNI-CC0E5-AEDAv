#ifndef __MERKLETREE_H__
#define __MERKLETREE_H__

// Merkle-AVL: árbol AVL donde cada nodo guarda un digest
//     hash(nodo) = combine(hash(izq), hashValue(valor), hash(der))
// de modo que el hash de la raíz resume todo el contenido (y la forma) del árbol.
//
// - BaseMerkleAVLTree<Traits>: reutilizable para cualquier T imprimible.
// - BaseMerkleMap<Traits>:     Map (clave -> valor) autenticado, construido sobre map.h.
//
// Notas:
// - Los hashes se recalculan de forma perezosa: las mutaciones solo marcan nodos
//   como "sucios" y rootHash() recalcula únicamente esos nodos.
// - La raíz depende de la FORMA del AVL: el mismo contenido insertado en otro
//   orden (o releído desde archivo) puede producir otra raíz. La copia clona la
//   estructura, por lo que conserva la raíz.
// - El hasher por defecto (FNV-1a de 64 bits) NO es criptográfico; puede
//   reemplazarse por cualquier política con la misma interfaz vía Traits.
// - El Ref de cada nodo no forma parte del hash: solo se autentica el valor.
// - Modificar valores sin pasar por el contenedor (referencias retenidas,
//   ApplyFunction/call) deja la caché desactualizada: verify() lo detecta.

#include "avltree.h"
#include "map.h"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Política de hash
// ---------------------------------------------------------------------------
template <typename T>
struct Fnv1aHasher {
    using Digest = std::uint64_t;

    static constexpr Digest kOffset = 14695981039346656037ULL;
    static constexpr Digest kPrime  = 1099511628211ULL;
    static constexpr unsigned char kLeafTag = 0x00; // separación de dominio
    static constexpr unsigned char kNodeTag = 0x01;

    static Digest mixByte(Digest h, unsigned char byte) {
        return (h ^ byte) * kPrime;
    }

    // Byte a byte (little-endian explícito) para que sea independiente de la plataforma
    static Digest mixDigest(Digest h, Digest d) {
        for (int i = 0; i < 8; ++i) h = mixByte(h, static_cast<unsigned char>(d >> (8 * i)));
        return h;
    }

    // Hash de un valor a partir de su representación textual (operator<<)
    static Digest hashValue(const T& value) {
        std::ostringstream text;
        text << value;
        Digest h = mixByte(kOffset, kLeafTag);
        for (unsigned char c : text.str()) h = mixByte(h, c);
        return h;
    }

    // Hash de un nodo interno a partir de sus hijos y su valor
    static Digest combine(Digest left, Digest entry, Digest right) {
        Digest h = mixByte(kOffset, kNodeTag);
        h = mixDigest(h, left);
        h = mixDigest(h, entry);
        return mixDigest(h, right);
    }
};

// ---------------------------------------------------------------------------
// Nodo, prueba de inclusión y traits
// ---------------------------------------------------------------------------
template <typename T, typename Hasher>
class MerkleNode : public BaseAVLNode<T, MerkleNode<T, Hasher>> {
public:
    using Parent = BaseAVLNode<T, MerkleNode<T, Hasher>>;
    using Digest = typename Hasher::Digest;
    using Parent::Parent;

    mutable Digest m_hash  = Digest{};
    mutable bool   m_dirty = true; // un nodo nuevo siempre debe calcularse
};

// Prueba de inclusión: hashes de los hijos del nodo y, por cada ancestro
// (de abajo hacia arriba), el hash de su valor y el de su otro hijo.
template <typename Digest>
struct MerkleProof {
    struct Step {
        Digest entry;    // hashValue del ancestro
        Digest sibling;  // hash del otro hijo del ancestro
        bool   fromLeft; // el hash acumulado entra como hijo izquierdo
    };

    Digest left  = Digest{};
    Digest right = Digest{};
    std::vector<Step> path;
};

template <typename T, typename H = Fnv1aHasher<T>>
struct MerkleTraits {
    using Hasher           = H;
    using Node             = MerkleNode<T, H>;
    using ForwardIterator  = BinaryTreeIterator<Node, false>;
    using BackwardIterator = BinaryTreeIterator<Node, true>;
};

template <typename T, typename H = Fnv1aHasher<T>>
struct MerkleAVLAscTraits : public AscendingTraits<T>, public MerkleTraits<T, H> {};

template <typename T, typename H = Fnv1aHasher<T>>
struct MerkleAVLDescTraits : public DescendingTraits<T>, public MerkleTraits<T, H> {};

// ---------------------------------------------------------------------------
// Árbol Merkle-AVL
// ---------------------------------------------------------------------------
template <typename Traits, template <typename> typename... Mixins>
class BaseMerkleAVLTree : public BaseAVLTree<Traits, Mixins...> {
public:
    using Parent     = BaseAVLTree<Traits, Mixins...>;
    using Node       = typename Parent::Node;
    using NodePtr    = typename Parent::NodePtr;
    using value_type = typename Parent::value_type;
    using Compare    = typename Traits::Compare;
    using Hasher     = typename Traits::Hasher;
    using Digest     = typename Hasher::Digest;
    using Proof      = MerkleProof<Digest>;

    BaseMerkleAVLTree() = default;

    BaseMerkleAVLTree(const BaseMerkleAVLTree& other) : Parent() { *this = other; }

    BaseMerkleAVLTree(BaseMerkleAVLTree&& other) noexcept : Parent(std::move(other)) {}

    BaseMerkleAVLTree(std::initializer_list<std::pair<value_type, Ref>> values) : Parent() {
        for (const auto& value : values)
            this->push_back(value.first, value.second);
    }

    // Copia estructural: conserva la forma y por tanto el hash raíz
    BaseMerkleAVLTree& operator=(const BaseMerkleAVLTree& other) {
        if (this == &other) return *this;
        std::scoped_lock lock(this->m_mutex, other.m_mutex);
        this->clear();
        this->m_root = clone(other.m_root, nullptr);
        this->m_size = other.m_size;
        return *this;
    }

    BaseMerkleAVLTree& operator=(BaseMerkleAVLTree&& other) noexcept {
        Parent::operator=(std::move(other));
        return *this;
    }

    Digest rootHash() const {
        auto lock = this->acquireLock();
        return digest(this->m_root);
    }

    // Recalcula todo desde cero (sin caché) y lo compara con la raíz en caché
    bool verify() const {
        auto lock = this->acquireLock();
        return recompute(this->m_root) == digest(this->m_root);
    }

    Proof proof(const value_type& value) const {
        auto lock = this->acquireLock();
        NodePtr node = this->m_root;
        while (node) {
            if (Compare()(value, node->getValue()))      node = node->m_pChild[0];
            else if (Compare()(node->getValue(), value)) node = node->m_pChild[1];
            else return proofOf(node);
        }
        throw std::out_of_range("MerkleTree::proof: valor no encontrado");
    }

    Proof proofOf(NodePtr node) const {
        auto lock = this->acquireLock();
        digest(this->m_root); // asegura la caché antes de leerla
        Proof p;
        p.left  = digest(node->m_pChild[0]);
        p.right = digest(node->m_pChild[1]);
        for (NodePtr child = node, parent = node->m_parent; parent; child = parent, parent = parent->m_parent) {
            const bool fromLeft = (child == parent->m_pChild[0]);
            p.path.push_back({Hasher::hashValue(parent->getValue()),
                              digest(parent->m_pChild[fromLeft ? 1 : 0]),
                              fromLeft});
        }
        return p;
    }

    static bool verifyProof(Digest root, const value_type& value, const Proof& p) {
        Digest h = Hasher::combine(p.left, Hasher::hashValue(value), p.right);
        for (const auto& step : p.path)
            h = step.fromLeft ? Hasher::combine(h, step.entry, step.sibling)
                              : Hasher::combine(step.sibling, step.entry, h);
        return h == root;
    }

    static std::string toHex(Digest d, size_t digits = 2 * sizeof(Digest)) {
        std::ostringstream text;
        text << std::hex << std::setfill('0') << std::setw(2 * sizeof(Digest)) << d;
        return text.str().substr(0, digits);
    }

    void onValueChanged(NodePtr node) override { markPath(node); }

protected:
    void onNodeChanged(NodePtr node) override { node->m_dirty = true; }

    NodePtr insertImpl(const value_type& value, Ref ref) override {
        NodePtr node = Parent::insertImpl(value, ref);
        markPath(node); // el rebalanceo puede detenerse antes de la raíz
        return node;
    }
    // removeNodeImpl: rebalanceFrom recorre hasta la raíz y llama a onNodeChanged

    std::string nodeLabel(NodePtr node) const override {
        return Parent::nodeLabel(node) + "#" + toHex(digest(node), 4);
    }

    static void markPath(NodePtr node) {
        for (; node; node = node->m_parent) node->m_dirty = true;
    }

    static Digest digest(NodePtr node) {
        if (!node) return Digest{};
        if (node->m_dirty) {
            node->m_hash  = Hasher::combine(digest(node->m_pChild[0]),
                                            Hasher::hashValue(node->getValue()),
                                            digest(node->m_pChild[1]));
            node->m_dirty = false;
        }
        return node->m_hash;
    }

    static Digest recompute(NodePtr node) {
        if (!node) return Digest{};
        return Hasher::combine(recompute(node->m_pChild[0]),
                               Hasher::hashValue(node->getValue()),
                               recompute(node->m_pChild[1]));
    }

    static NodePtr clone(NodePtr src, NodePtr parent) {
        if (!src) return nullptr;
        NodePtr node    = new Node(src->getValue(), src->getRef());
        node->m_height  = src->m_height;
        node->m_hash    = src->m_hash;
        node->m_dirty   = src->m_dirty;
        node->m_parent  = parent;
        node->m_pChild[0] = clone(src->m_pChild[0], node);
        node->m_pChild[1] = clone(src->m_pChild[1], node);
        return node;
    }
};

template <typename Traits>
using MerkleAVLTree = BaseMerkleAVLTree<Traits, Callable, Applicable, Searchable, ReverseCallable, Printable, Readable>;

template <typename T> using AscendingMerkleAVLTree  = MerkleAVLTree<MerkleAVLAscTraits<T>>;
template <typename T> using DescendingMerkleAVLTree = MerkleAVLTree<MerkleAVLDescTraits<T>>;

// ---------------------------------------------------------------------------
// Merkle Map: Map de map.h con un Merkle-AVL como árbol interno
// ---------------------------------------------------------------------------
template <typename _Key, typename _Value, typename H = Fnv1aHasher<MapEntry<_Key, _Value>>>
struct MerkleMapTraits : public AscendingTraits<MapEntry<_Key, _Value>>,
                         public MerkleTraits<MapEntry<_Key, _Value>, H> {
    using Key   = _Key;
    using Value = _Value;
    using Tree  = MapTree<MerkleMapTraits, BaseMerkleAVLTree<MerkleMapTraits>>;
};

template <typename Traits, template <typename> typename... Mixins>
class BaseMerkleMap : public BaseMap<Traits, Mixins...> {
public:
    using Parent     = BaseMap<Traits, Mixins...>;
    using Key        = typename Parent::Key;
    using Value      = typename Parent::Value;
    using value_type = typename Parent::value_type;
    using Tree       = typename Parent::Tree;
    using NodePtr    = typename Parent::NodePtr;
    using Digest     = typename Tree::Digest;
    using Proof      = typename Tree::Proof;
    using Parent::Parent;

protected:
    using typename Parent::Lock;

public:

    Digest rootHash()   const { Lock lock(this->m_mutex); return this->m_tree.rootHash(); }
    bool   verify()     const { Lock lock(this->m_mutex); return this->m_tree.verify(); }
    size_t height()     const { Lock lock(this->m_mutex); return this->m_tree.height(); }
    bool   isBalanced() const { Lock lock(this->m_mutex); return this->m_tree.isBalanced(); }

    Proof proof(const Key& key) const {
        Lock lock(this->m_mutex);
        if (NodePtr n = this->m_tree.findKey(key))
            return this->m_tree.proofOf(n);
        throw std::out_of_range("MerkleMap::proof: clave no encontrada");
    }

    static bool verifyProof(Digest root, const Key& key, const Value& value, const Proof& p) {
        return Tree::verifyProof(root, value_type(key, value), p);
    }

    static std::string toHex(Digest d, size_t digits = 2 * sizeof(Digest)) {
        return Tree::toHex(d, digits);
    }
};

template <typename Key, typename Value>
using MerkleMap = BaseMerkleMap<MerkleMapTraits<Key, Value>, Callable, Printable, Readable>;

#endif // __MERKLETREE_H__
