#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "avl.h"
#include "traits.h"
#include <cstddef>
#include <istream>
#include <ostream>
#include <utility>

template <typename Key, typename Value>
class MapEntry {
public:
    using Delim = StreamDelimiters::Delim;
    static constexpr Delim entry_open = '{';
    static constexpr Delim entry_close = '}';
    static constexpr Delim key_sep = ':';

    MapEntry() = default;

    MapEntry(const Key& key, const Value& value = Value{}) : m_key(key), m_value(value) {}

    const Key& key() const { return m_key; }
    const Value& value() const { return m_value; }
    Value& value() { return m_value; }

    friend bool operator<(const MapEntry& lhs, const MapEntry& rhs) { return lhs.m_key < rhs.m_key; }

    friend std::ostream& operator<<(std::ostream& os, const MapEntry& entry) {
        return os << entry_open << ' ' << entry.m_key << ' ' << key_sep << ' '
                  << entry.m_value << ' ' << entry_close;
    }

    friend std::istream& operator>>(std::istream& is, MapEntry& entry) {
        using SD = StreamDelimiters;
        Key key;
        Value value;
        if (SD::expect(is, entry_open) && is >> key && SD::expect(is, key_sep) &&
            is >> value && SD::expect(is, entry_close))
            entry = MapEntry(key, value);
        return is;
    }

private:
    Key m_key{};
    Value m_value{};
};

template <typename Key, typename Value> class HashTable {
public:
    using Entry = MapEntry<Key, Value>;
    using Tree = AVLTree<AVLTraits<Entry>>;
    using value_type = typename Tree::value_type;
    using Compare = typename Tree::Compare;

private:
    Tree m_tree;

public:
    Value& operator[](const Key& key) { return m_tree[Entry(key)].value(); }

    bool insert(const Key& key, const Value& value) {
        return m_tree.insert_unique(Entry(key, value), Ref{}).second;
    }

    template <typename Self> auto find(this Self& self, const Key& key) {
        auto entry = self.m_tree.find(Entry(key));
        return entry ? &entry->value() : nullptr;
    }

    bool contains(const Key& key) const { return m_tree.contains(Entry(key)); }
    bool remove(const Key& key) { return m_tree.remove(Entry(key)); }

    bool empty() const { return m_tree.empty(); }
    std::size_t size() const { return m_tree.size(); }
    void clear() { m_tree.clear(); }

    auto PreOrder() const { return m_tree.PreOrder(); }

    void print_tree(std::ostream& os, visual::Layout layout = visual::Layout::Vertical) const {
        m_tree.print_tree(os, layout);
    }

    friend std::ostream& operator<<(std::ostream& os, const HashTable& table) { return os << table.m_tree; }
    friend std::istream& operator>>(std::istream& is, HashTable& table) { return is >> table.m_tree; }
};

#endif // !__HASHTABLE_H__
