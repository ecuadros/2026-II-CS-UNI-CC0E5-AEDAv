#ifndef CONTAINERS_TREEITERATOR_H
#define CONTAINERS_TREEITERATOR_H

#include "GeneralIterator.h"

template <typename Node, bool Reverse = false>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<Node, Reverse>, Node> {
public:
    using value_type        = Node;
    using MySelf            = BinaryTreeIterator<Node, Reverse>;
    using Parent            = GeneralIterator<MySelf, Node>;
    using Parent::Parent; // Inherit constructor

    // Prefix increment
    BinaryTreeIterator& operator++() {
        Node* cur = Parent::m_ptr;
        if (cur->m_pChild[Reverse ? 0 : 1]) {
            cur = cur->m_pChild[Reverse ? 0 : 1];
            while (cur->m_pChild[Reverse ? 1 : 0])
                cur = cur->m_pChild[Reverse ? 1 : 0];
            Parent::m_ptr = cur;
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
};

#endif // CONTAINERS_TREEITERATOR_H
