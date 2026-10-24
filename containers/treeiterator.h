#ifndef CONTAINERS_TREEITERATOR_H
#define CONTAINERS_TREEITERATOR_H

#include "GeneralIterator.h"

template <typename Node, bool Reverse = false>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<Node, Reverse>, Node> {
    using Parent = GeneralIterator<BinaryTreeIterator<Node, Reverse>, Node>;

    static Node* extreme(Node* node) {
        while (node && node->m_pChild[Reverse ? 1 : 0])
            node = node->m_pChild[Reverse ? 1 : 0];
        return node;
    }

public:
    BinaryTreeIterator() : Parent(nullptr) {}

    explicit BinaryTreeIterator(Node* root) : Parent(extreme(root)) {}

    BinaryTreeIterator& operator++() {
        Node* cur = Parent::m_ptr;
        if (cur->m_pChild[Reverse ? 0 : 1]) {
            Parent::m_ptr = extreme(cur->m_pChild[Reverse ? 0 : 1]);
        } else {
            Node* p = cur->m_pParent;
            while (p && cur == p->m_pChild[Reverse ? 0 : 1]) {
                cur = p;
                p = p->m_pParent;
            }
            Parent::m_ptr = p;
        }
        return *this;
    }

    BinaryTreeIterator operator++(int) {
        auto previous = *this;
        ++*this;
        return previous;
    }
};

#endif // CONTAINERS_TREEITERATOR_H
