#ifndef CONTAINERS_TREEITERATOR_H
#define CONTAINERS_TREEITERATOR_H

#include "GeneralIterator.h"

template <typename Node, bool Reverse = false>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<Node, Reverse>, Node> {
    Node* extreme(Node* cur) const {
        while (cur && cur->m_pChild[Reverse])
            cur = cur->m_pChild[Reverse];
        return cur;
    }

public:
    using value_type        = Node;
    using MySelf            = BinaryTreeIterator<Node, Reverse>;
    using Parent            = GeneralIterator<MySelf, Node>;

    BinaryTreeIterator(Node* root = nullptr) : Parent(extreme(root)) {}

    // Prefix increment
    BinaryTreeIterator& operator++() {
        Node* cur = Parent::m_ptr;
        if (cur->m_pChild[!Reverse]) {
            Parent::m_ptr = extreme(cur->m_pChild[!Reverse]);
        } else {
            Node* p = cur->m_pParent;
            while (p && cur == p->m_pChild[!Reverse]) {
                cur = p;
                p = p->m_pParent;
            }
            Parent::m_ptr = p;
        }
        return *this;
    }
};

#endif // CONTAINERS_TREEITERATOR_H
