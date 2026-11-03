#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "avl.h"
#include "traits.h"
#include <cstddef>
#include <initializer_list>
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
    using Node = typename Tree::Node;
    using Compare = typename Tree::Compare;
    using ForwardIterator = typename Tree::ForwardIterator;
    using BackwardIterator = typename Tree::BackwardIterator;

private:
    Tree m_tree;

public:
    HashTable() = default;

    HashTable(std::initializer_list<std::pair<Entry, Ref>> values) : m_tree(values) {}

    Value& operator[](const Key& key) { return m_tree.insert_unique(Entry(key), Ref{}).first->value(); }

    template <typename Self> auto find(this Self& self, const Key& key) {
        auto entry = self.m_tree.find(Entry(key));
        return entry ? &entry->value() : nullptr;
    }

    bool contains(const Key& key) const { return m_tree.contains(Entry(key)); }
    bool remove(const Key& key) { return m_tree.remove(Entry(key)); }

    void insert(const Entry& entry, Ref ref) { m_tree.insert(entry, ref); }
    void push_back(const Entry& entry, Ref ref) { m_tree.push_back(entry, ref); }

    bool empty() const { return m_tree.empty(); }
    std::size_t size() const { return m_tree.size(); }
    std::size_t height() const { return m_tree.height(); }
    void clear() { m_tree.clear(); }

    ForwardIterator begin() const { return m_tree.begin(); }
    ForwardIterator end() const { return m_tree.end(); }
    BackwardIterator rbegin() const { return m_tree.rbegin(); }
    BackwardIterator rend() const { return m_tree.rend(); }

    auto InOrder() const { return m_tree.InOrder(); }
    auto ReverseInOrder() const { return m_tree.ReverseInOrder(); }
    auto PreOrder() const { return m_tree.PreOrder(); }
    auto ReversePreOrder() const { return m_tree.ReversePreOrder(); }
    auto PostOrder() const { return m_tree.PostOrder(); }
    auto ReversePostOrder() const { return m_tree.ReversePostOrder(); }

    template <typename Self, typename... Args> decltype(auto) call(this Self& self, Args&&... args) {
        return self.m_tree.call(std::forward<Args>(args)...);
    }

    template <typename Self, typename... Args> decltype(auto) rcall(this Self& self, Args&&... args) {
        return self.m_tree.rcall(std::forward<Args>(args)...);
    }

    template <typename Self, typename... Args> decltype(auto) FirstThat(this Self& self, Args&&... args) {
        return self.m_tree.FirstThat(std::forward<Args>(args)...);
    }

    template <typename Self, typename... Args> decltype(auto) RFirstThat(this Self& self, Args&&... args) {
        return self.m_tree.RFirstThat(std::forward<Args>(args)...);
    }

    auto tree_view() const { return m_tree.tree_view(); }

    void print_tree(std::ostream& os, visual::Layout layout = visual::Layout::Vertical) const {
        m_tree.print_tree(os, layout);
    }

    void visualize(std::ostream& os) const { m_tree.visualize(os); }

    friend std::ostream& operator<<(std::ostream& os, const HashTable& table) { return os << table.m_tree; }
    friend std::istream& operator>>(std::istream& is, HashTable& table) { return is >> table.m_tree; }
};

#endif // !__HASHTABLE_H__
