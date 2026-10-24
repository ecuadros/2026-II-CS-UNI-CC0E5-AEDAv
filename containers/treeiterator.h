#ifndef CONTAINERS_TREEITERATOR_H
#define CONTAINERS_TREEITERATOR_H

#include "GeneralIterator.h"
#include <stack>

template <typename Node, bool Reverse = false>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<Node, Reverse>, Node> {
    using Parent = GeneralIterator<BinaryTreeIterator<Node, Reverse>, Node>;
    std::stack<Node*> m_pending;

    void pushPath(Node* node) {
        while (node) {
            m_pending.push(node);
            node = node->m_pChild[Reverse ? 1 : 0];
        }
        Parent::m_ptr = m_pending.empty() ? nullptr : m_pending.top();
    }

public:
    BinaryTreeIterator() : Parent(nullptr) {}

    explicit BinaryTreeIterator(Node* root) : Parent(nullptr) { pushPath(root); }

    BinaryTreeIterator& operator++() {
        Node* node = m_pending.top();
        m_pending.pop();
        pushPath(node->m_pChild[Reverse ? 0 : 1]);
        return *this;
    }

    BinaryTreeIterator operator++(int) {
        auto previous = *this;
        ++*this;
        return previous;
    }
};

#endif // CONTAINERS_TREEITERATOR_H
