#ifndef __HASH_H__
#define __HASH_H__
#include "avl.h"

template <typename Key>
class Hash {
    struct Tree : public AVL<AVLAscTraits<Key>> {
        void push_back(const Key& key, Ref ref) override { this->find_or_insert(key).ref() = ref; }
    };
    Tree m_tree;

public:
    Ref& operator[](const Key& key) { return m_tree.find_or_insert(key).ref(); }
    void print(std::ostream& os) const { m_tree.print(os); }

    friend std::ostream& operator<<(std::ostream& os, const Hash& hash) { return os << hash.m_tree; }
    friend std::istream& operator>>(std::istream& is, Hash& hash) { return is >> hash.m_tree; }
};

#endif // __HASH_H__
