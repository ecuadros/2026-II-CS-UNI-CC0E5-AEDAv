#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "avl.h"
#include "traits.h"
#include <cstddef>
#include <initializer_list>
#include <istream>
#include <ostream>
#include <utility>

template <typename Key, typename Value, typename Comparator = std::less<Key>> class HashTable {
public:
    using Tree = AVLTree<AVLTraits<Key, Comparator, Value>>;
    using value_type = typename Tree::value_type;
    using ref_type = typename Tree::ref_type;
    using Compare = typename Tree::Compare;

private:
    Tree m_tree;

public:
    HashTable() = default;
    HashTable(std::initializer_list<std::pair<Key, Value>> values) {
        for (const auto& [key, value] : values)
            m_tree.insert_unique(key, value);
    }

    auto begin() const { return m_tree.begin(); }
    auto end() const { return m_tree.end(); }
    auto rbegin() const { return m_tree.rbegin(); }
    auto rend() const { return m_tree.rend(); }

    Value& operator[](const Key& key) { return m_tree[key]; }

    bool insert(const Key& key, const Value& value) { return m_tree.insert_unique(key, value).second; }

    template <typename Self> auto find(this Self& self, const Key& key) { return self.m_tree.find_ref(key); }

    bool contains(const Key& key) const { return m_tree.contains(key); }
    bool remove(const Key& key) { return m_tree.remove(key); }

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
