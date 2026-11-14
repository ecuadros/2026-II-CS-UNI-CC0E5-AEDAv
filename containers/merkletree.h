#ifndef __MERKLETREE_H__
#define __MERKLETREE_H__
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include "binarytree.h"
#include "vector.h"
#include "../utilities/canvas.h"
#include "../utilities/sha256.h"

template <typename Traits, template <typename> typename... Mixins>
class BaseMerkleTree;

template <typename T>
using MerkleTree = BaseMerkleTree<VectorAscTraits<T>, Callable, Searchable, ReverseCallable, Printable, Readable>;

template <typename Traits, template <typename> typename... Mixins>
class BaseMerkleTree : public GeneralContainer<Traits, Mixins...> {
public:
    using Parent = GeneralContainer<Traits, Mixins...>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
    using Digest = std::string;
    using Digests = Vector<VectorAscTraits<Digest>>;

private:
    Vector<Traits> m_leaves;
    Digests m_hashes;

    size_t capacity() const { return (m_hashes.size() + 1) / 2; }
    static std::string text(const Node& leaf) {
        std::ostringstream os;
        os << leaf;
        return os.str();
    }
    static Digest digest(const Node& leaf) { return Sha256(text(leaf)); }
    static Digest combine(const Digest& left, const Digest& right) { return right.empty() ? left : Sha256(left + right); }
    void rehash(size_t node) { m_hashes[node].value() = combine(m_hashes[2 * node + 1].getValue(), m_hashes[2 * node + 2].getValue()); }
    void rehash_path(size_t index);
    void rebuild();

public:
    BaseMerkleTree() = default;
    BaseMerkleTree(const BaseMerkleTree& other) : ContainerCore<Traits>(other) { *this = other; }
    BaseMerkleTree(BaseMerkleTree&& other) noexcept { *this = std::move(other); }
    BaseMerkleTree& operator=(const BaseMerkleTree& other);
    BaseMerkleTree& operator=(BaseMerkleTree&& other) noexcept;

    void push_back(const value_type& value, Ref ref) override;
    void update(size_t index, const value_type& value, Ref ref);
    void clear() override;

    size_t size() const;
    Digest root() const;
    Digests proof(size_t index) const;
    static bool verify(const Node& leaf, const Digests& proof, const Digest& root);
    void print(std::ostream& os) const;

    ForwardIterator begin() const override { return m_leaves.begin(); }
    ForwardIterator end() const override { return m_leaves.end(); }
    BackwardIterator rbegin() const override { return m_leaves.rbegin(); }
    BackwardIterator rend() const override { return m_leaves.rend(); }
};

template <typename Traits, template <typename> typename... Mixins>
auto BaseMerkleTree<Traits, Mixins...>::operator=(const BaseMerkleTree& other) -> BaseMerkleTree& {
    if (this == &other)
        return *this;
    std::scoped_lock lock(this->m_mutex, other.m_mutex);
    m_leaves = other.m_leaves;
    m_hashes = other.m_hashes;
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseMerkleTree<Traits, Mixins...>::operator=(BaseMerkleTree&& other) noexcept -> BaseMerkleTree& {
    this->move_from(other, [&]{
        m_leaves = std::move(other.m_leaves);
        m_hashes = std::move(other.m_hashes);
    });
    return *this;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseMerkleTree<Traits, Mixins...>::push_back(const value_type& value, Ref ref) {
    std::lock_guard lock(this->m_mutex);
    m_leaves.push_back(value, ref);
    if (m_leaves.size() > capacity())
        rebuild();
    else
        rehash_path(m_leaves.size() - 1);
}

template <typename Traits, template <typename> typename... Mixins>
void BaseMerkleTree<Traits, Mixins...>::update(size_t index, const value_type& value, Ref ref) {
    std::lock_guard lock(this->m_mutex);
    m_leaves[index] = Node(value, ref);
    rehash_path(index);
}

template <typename Traits, template <typename> typename... Mixins>
void BaseMerkleTree<Traits, Mixins...>::rehash_path(size_t index) {
    size_t node = capacity() - 1 + index;
    m_hashes[node].value() = digest(m_leaves[index]);
    while (node > 0) {
        node = (node - 1) / 2;
        rehash(node);
    }
}

template <typename Traits, template <typename> typename... Mixins>
void BaseMerkleTree<Traits, Mixins...>::rebuild() {
    size_t cap = 1;
    while (cap < m_leaves.size())
        cap *= 2;
    m_hashes.clear();
    for (size_t node = 0; node < 2 * cap - 1; ++node)
        m_hashes.push_back(Digest(), 0);
    for (size_t index = 0; index < m_leaves.size(); ++index)
        m_hashes[cap - 1 + index].value() = digest(m_leaves[index]);
    for (size_t node = cap - 1; node > 0; --node)
        rehash(node - 1);
}

template <typename Traits, template <typename> typename... Mixins>
void BaseMerkleTree<Traits, Mixins...>::clear() {
    std::lock_guard lock(this->m_mutex);
    m_leaves.clear();
    m_hashes.clear();
}

template <typename Traits, template <typename> typename... Mixins>
size_t BaseMerkleTree<Traits, Mixins...>::size() const {
    std::lock_guard lock(this->m_mutex);
    return m_leaves.size();
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseMerkleTree<Traits, Mixins...>::root() const -> Digest {
    std::lock_guard lock(this->m_mutex);
    return m_hashes.empty() ? Digest() : m_hashes[0].getValue();
}

template <typename Traits, template <typename> typename... Mixins>
auto BaseMerkleTree<Traits, Mixins...>::proof(size_t index) const -> Digests {
    std::lock_guard lock(this->m_mutex);
    if (index >= m_leaves.size())
        throw std::out_of_range("Indice fuera de rango");
    Digests siblings;
    for (size_t node = capacity() - 1 + index; node > 0; node = (node - 1) / 2) {
        Side side = node % 2 ? Right : Left;
        const Digest& sibling = m_hashes[side == Right ? node + 1 : node - 1].getValue();
        if (!sibling.empty())
            siblings.push_back(sibling, side);
    }
    return siblings;
}

template <typename Traits, template <typename> typename... Mixins>
bool BaseMerkleTree<Traits, Mixins...>::verify(const Node& leaf, const Digests& proof, const Digest& root) {
    Digest hash = digest(leaf);
    for (const auto& sibling : proof)
        hash = sibling.getRef() == Right ? combine(hash, sibling.getValue()) : combine(sibling.getValue(), hash);
    return hash == root;
}

template <typename Traits, template <typename> typename... Mixins>
void BaseMerkleTree<Traits, Mixins...>::print(std::ostream& os) const {
    std::lock_guard lock(this->m_mutex);
    const size_t label = 4;
    size_t widest = label;
    for (const auto& leaf : m_leaves)
        widest = std::max(widest, text(leaf).size());
    size_t width = widest + 2 + widest % 2;
    size_t cap = capacity();
    Canvas canvas;
    for (size_t level = 0, first = 0; first < m_hashes.size(); ++level, first = 2 * first + 1) {
        size_t span = (cap >> level) * width;
        for (size_t node = first; node <= 2 * first; ++node) {
            const Digest& hash = m_hashes[node].getValue();
            if (hash.empty())
                continue;
            size_t x = (node - first) * span + span / 2;
            canvas.print(x - label / 2, 2 * level, hash.substr(0, label));
            if (node >= cap - 1) {
                std::string data = text(m_leaves[node - cap + 1]);
                canvas.print(x - data.size() / 2, 2 * level + 1, data);
            } else {
                size_t left = x - span / 4;
                size_t right = m_hashes[2 * node + 2].getValue().empty() ? x : x + span / 4;
                canvas.print(left, 2 * level + 1, "+" + std::string(right - left - 1, '-') + "+");
            }
        }
    }
    os << canvas;
}

#endif // __MERKLETREE_H__
