#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "avl.h"
#include "traits.h"
#include <cstddef>
#include <functional>
#include <istream>
#include <ostream>
#include <tuple>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class HashEntry {
public:
    using Delim = StreamDelimiters::Delim;
    static constexpr Delim entry_open = '{';
    static constexpr Delim entry_close = '}';
    static constexpr Delim key_sep = ':';

    HashEntry() = default;

    HashEntry(const Key& key, const Value& value = Value{})
        : m_hash(Hash{}(key)), m_key(key), m_value(value) {}

    std::size_t hash() const { return m_hash; }
    const Key& key() const { return m_key; }
    const Value& value() const { return m_value; }
    Value& value() { return m_value; }

    friend auto operator<=>(const HashEntry& lhs, const HashEntry& rhs) {
        return std::tie(lhs.m_hash, lhs.m_key) <=> std::tie(rhs.m_hash, rhs.m_key);
    }

    friend std::ostream& operator<<(std::ostream& os, const HashEntry& entry) {
        return os << entry_open << ' ' << entry.m_key << ' ' << key_sep << ' '
                  << entry.m_value << ' ' << entry_close;
    }

    friend std::istream& operator>>(std::istream& is, HashEntry& entry) {
        using SD = StreamDelimiters;
        Key key;
        Value value;
        if (SD::expect(is, entry_open) && is >> key && SD::expect(is, key_sep) &&
            is >> value && SD::expect(is, entry_close))
            entry = HashEntry(key, value);
        return is;
    }

private:
    std::size_t m_hash = 0;
    Key m_key{};
    Value m_value{};
};

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class HashTable : public AVLTree<AVLTraits<HashEntry<Key, Value, Hash>>> {
public:
    using Entry = HashEntry<Key, Value, Hash>;
    using Parent = AVLTree<AVLTraits<Entry>>;

    Value& operator[](const Key& key) {
        return this->insert_unique(Entry(key), Ref{}).first->value();
    }

    template <typename Self> auto find(this Self& self, const Key& key) {
        auto entry = self.Parent::find(Entry(key));
        return entry ? &entry->value() : nullptr;
    }

    bool contains(const Key& key) const { return Parent::contains(Entry(key)); }
    bool remove(const Key& key) { return Parent::remove(Entry(key)); }
};

#endif // !__HASHTABLE_H__
