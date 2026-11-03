#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "avl.h"
#include "traits.h"
#include <istream>
#include <ostream>

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

template <typename Key, typename Value>
class HashTable : public AVLTree<AVLTraits<MapEntry<Key, Value>>> {
public:
    using Entry = MapEntry<Key, Value>;
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
