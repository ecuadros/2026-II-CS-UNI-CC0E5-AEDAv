#pragma once

#include <unordered_map>
#include <sstream>
#include <vector>
#include <string>

#include "avltree.h"
#include "GeneralContainer.h"

#include <compare>
#include <functional>
#include <iomanip>
#include <istream>
#include <mutex>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

template <typename keyT, typename valueT>
class MapEntry {
public:
    using Key      = keyT;
    using Value    = valueT;
    using Delim    = std::istream::char_type;
    using Text     = std::basic_string<Delim>;
    using TextView = std::basic_string_view<Delim>;
    using Position = std::istream::pos_type;

    // Delimitadores del formato (clave:valor)
    static constexpr Delim Open      = '(';
    static constexpr Delim Separator = ':';
    static constexpr Delim Close     = ')';
    static constexpr Delim Quote     = '"';

private:
    Key   m_key{};
    Value m_value{};

    template <typename T>
    static void write(std::ostream& os, const T& elem) {
        if constexpr (std::is_convertible_v<const T&, TextView>) {
            os << std::quoted(TextView(elem), Quote);
        } else {
            os << elem;
        }
    }

    template <typename T>
    static std::istream& read(std::istream& is, T& elem) {
        if constexpr (std::is_same_v<T, Text>) {
            Delim first{};
            if (!(is >> first)) {
                return is;
            }
            if (first != Quote) {
                is.setstate(std::ios::failbit);
                return is;
            }
            is.unget();
            return is >> std::quoted(elem, Quote);
        } else {
            return is >> elem;
        }
    }

public:
    MapEntry() = default;
    explicit MapEntry(const Key& key) : m_key(key) {}
    MapEntry(const Key &key, const Value &value) : m_key(key), m_value(value) {}
    
    const Key&   key()   const { return m_key; }
    Value&       value()       { return m_value; }
    const Value& value() const { return m_value; }

    friend bool operator==(const MapEntry &a, const MapEntry &b) { return a.m_key == b.m_key; }
    friend bool operator< (const MapEntry& a, const MapEntry& b) { return a.m_key <  b.m_key; }
    friend auto operator<=>(const MapEntry &a, const MapEntry &b) requires std::three_way_comparable<Key> { return a.m_key <=> b.m_key; }

    friend std::ostream &operator<<(std::ostream &os, const MapEntry &entry){
        os << Open;
        write(os, entry.m_key);
        os << Separator;
        write(os, entry.m_value);
        return os << Close;
    }

    friend std::istream& operator>>(std::istream& is, MapEntry& e) {
        const Position start = is.tellg();
        Key   key{};
        Value value{};
        Delim open{}, separator{}, close{};

        const auto valid = (is >> open) && open == Open
                        && read(is, key)
                        && (is >> separator) && separator == Separator
                        && read(is, value)
                        && (is >> close) && close == Close;

        if (valid) {
            e.m_key   = std::move(key);
            e.m_value = std::move(value);
        } else {
            if (start != Position(-1)) {
                is.clear();
                is.seekg(start);
            }
            is.setstate(std::ios::failbit);
        }
        return is;
    }
};

template <typename _Key, typename _Value>
struct MapTraits : public AVLTreeAscTraits<MapEntry<_Key, _Value>> {
    using Key   = _Key;
    using Value = _Value;
};

template <typename Traits>
class MapTree : public BaseAVLTree<Traits> {
public:
    using Parent     = BaseAVLTree<Traits>;
    using NodePtr    = typename Parent::NodePtr;
    using value_type = typename Parent::value_type;
    using Key        = typename Traits::Key;
    using Parent::Parent;
private:
    NodePtr m_lastInserted = nullptr;
public:
    NodePtr findKey(const Key& key) const {
        NodePtr cur = this->m_root;
        while (cur) {
            const Key& k = cur->value().key();
            if (key < k){
                cur = cur->m_pChild[0];
            } else if (k < key){
                cur = cur->m_pChild[1];
            } else {
                return cur;
            }
        }
        return nullptr;
    }

    NodePtr insertNew(const value_type& entry, Ref ref) {
        this->insert(entry, ref);
        return std::exchange(m_lastInserted, nullptr);
    }

protected:
    NodePtr insertImpl(const value_type& value, Ref ref) override {
        m_lastInserted = Parent::insertImpl(value, ref);
        return m_lastInserted;
    }
};  

template <typename Traits, template <typename> typename... Mixins>
class BaseMap : public GeneralContainer<Traits, Mixins...> {
public:
    using Parent           = GeneralContainer<Traits, Mixins...>;
    using Core             = ContainerCore<Traits>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
    using Key              = typename Traits::Key;
    using Value            = typename Traits::Value;
    using Tree             = MapTree<Traits>;
    using NodePtr          = typename Tree::NodePtr;

protected:
    using Lock = std::lock_guard<std::recursive_mutex>;
    using Core::m_mutex;
    
    Tree m_tree;

public:
    BaseMap() = default;

    // Sin delegar en BaseMap(): en una clase derivada, la inicialización por valor
    // del subobjeto base pondría a cero el recursive_mutex de la base virtual ya construida.
    BaseMap(std::initializer_list<std::pair<Key, Value>> values) {
        for (const auto& [k, v] : values) push_back(value_type(k, v));
    }

    BaseMap(const BaseMap& other) : Core(), Parent() {
        Lock lock(other.m_mutex);
        m_tree = other.m_tree;
    }

    BaseMap(BaseMap&& other) noexcept : Core(), Parent() {
        Lock lock(other.m_mutex);
        m_tree = std::move(other.m_tree);
    }

    BaseMap& operator=(const BaseMap& other) {
        if (this == &other) return *this;
        std::scoped_lock lock(m_mutex, other.m_mutex);
        m_tree = other.m_tree;
        return *this;
    }

    BaseMap& operator=(BaseMap&& other) noexcept {
        if (this == &other) return *this;
        std::scoped_lock lock(m_mutex, other.m_mutex);
        m_tree = std::move(other.m_tree);
        return *this;
    }

    ~BaseMap() override = default;

    Value& operator[](const Key &key){
        Lock lock(m_mutex);
        if (NodePtr n = m_tree.findKey(key)){
            return n->value().value();
        }
        return m_tree.insertNew(value_type(key), 0)->value().value();
    }

    // Interfaz de ContainerCore (la usa Readable): equivale a insert_or_assign.
    void push_back(const value_type& entry, Ref ref = 0) override {
        Lock lock(m_mutex);
        if (NodePtr n = m_tree.findKey(entry.key())) {
            n->value().value() = entry.value();
            return;
        }
        m_tree.insertNew(entry, ref);
    }

    Value& at(const Key &key) {
        Lock lock(m_mutex);
        if (NodePtr n = m_tree.findKey(key)){
            return n->value().value();
        }
        throw std::out_of_range("Map::at: clave no encontrada");
    }

    const Value& at(const Key& key) const {
        Lock lock(m_mutex);
        if (NodePtr n = m_tree.findKey(key)){
            return n->value().value();
        }
        throw std::out_of_range("Map::at: clave no encontrada");
    }

    std::optional<Value> get(const Key& key) const {
        Lock lock(m_mutex);
        if (NodePtr n = m_tree.findKey(key)){
            return n->value().value();
        }
        return std::nullopt;
    }

    bool contains(const Key& key) const {
        Lock lock(m_mutex);
        return m_tree.findKey(key) != nullptr;
    }

    bool erase(const Key& key) {
        Lock lock(m_mutex);
        NodePtr n = m_tree.findKey(key);
        if (!n){
            return false;
        }
        m_tree.remove(n->value());
        return true;
    }    

    void clear() override {
        Lock lock(m_mutex);
        m_tree.clear();
    }

    size_t size() const {
        Lock lock(m_mutex);
        return m_tree.size();
    }

    bool empty() const override {
        Lock lock(m_mutex);
        return m_tree.size() == 0;
    }

    ForwardIterator  begin()  const override { Lock lock(m_mutex); return m_tree.begin(); }
    ForwardIterator  end()    const override { return m_tree.end(); }
    BackwardIterator rbegin() const          { Lock lock(m_mutex); return m_tree.rbegin(); }
    BackwardIterator rend()   const          { return m_tree.rend(); }
};

template <typename Key, typename Value>
using Map = BaseMap<MapTraits<Key, Value>, Callable, Printable, Readable>;
