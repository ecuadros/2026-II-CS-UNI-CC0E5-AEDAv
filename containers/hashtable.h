#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "GeneralNode.h"
#include "aliases.h"
#include <compare>
#include <cstddef>
#include <functional>
#include <istream>
#include <ostream>

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class HashEntry {
public:
  using Delim = StreamDelimiters::Delim;
  static constexpr Delim entry_open = '{';
  static constexpr Delim entry_close = '}';
  static constexpr Delim key_sep = ':';

  HashEntry() = default;

  HashEntry(const Key &key, const Value &value = Value{})
      : m_hash(Hash{}(key)), m_key(key), m_value(value) {}

  std::size_t hash() const { return m_hash; }
  const Key &key() const { return m_key; }
  const Value &value() const { return m_value; }
  Value &value() { return m_value; }

  friend std::strong_ordering operator<=>(const HashEntry &lhs,
                                          const HashEntry &rhs) {
    if (auto cmp = lhs.m_hash <=> rhs.m_hash; cmp != 0)
      return cmp;
    if (lhs.m_key < rhs.m_key)
      return std::strong_ordering::less;
    if (rhs.m_key < lhs.m_key)
      return std::strong_ordering::greater;
    return std::strong_ordering::equal;
  }

  friend bool operator==(const HashEntry &lhs, const HashEntry &rhs) {
    return lhs.m_hash == rhs.m_hash && lhs.m_key == rhs.m_key;
  }

  friend std::ostream &operator<<(std::ostream &os, const HashEntry &entry) {
    return os << entry_open << ' ' << entry.m_key << ' ' << key_sep << ' '
              << entry.m_value << ' ' << entry_close;
  }

  friend std::istream &operator>>(std::istream &is, HashEntry &entry) {
    using SD = StreamDelimiters;
    if (SD::expect(is, entry_open) && is >> entry.m_key &&
        SD::expect(is, key_sep) && is >> entry.m_value &&
        SD::expect(is, entry_close))
      entry.m_hash = Hash{}(entry.m_key);
    return is;
  }

private:
  std::size_t m_hash = 0;
  Key m_key{};
  Value m_value{};
};

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class HashTable : public AVLSearchTree<HashEntry<Key, Value, Hash>> {
public:
  using Entry = HashEntry<Key, Value, Hash>;

  Value &operator[](const Key &key) {
    Entry entry(key);
    return this->insert_unique(entry, static_cast<Ref>(entry.hash()))
        .first->value();
  }
};

#endif // !__HASHTABLE_H__
