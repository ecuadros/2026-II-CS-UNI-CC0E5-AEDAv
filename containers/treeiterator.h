#ifndef CONTAINERS_TREEITERATOR_H
#define CONTAINERS_TREEITERATOR_H

#include "GeneralIterator.h"
#include <stack>

template <typename Node, bool Reverse = false>
class BinaryTreeIterator : public GeneralIterator<BinaryTreeIterator<Node, Reverse>, Node> {
    using Parent = GeneralIterator<BinaryTreeIterator<Node, Reverse>, Node>;
    std::stack<Node*> m_path;

    void pushPath(Node* node) {
        while (node) {
            m_path.push(node);
            node = Reverse ? node->m_pChild[1]
                           : node->m_pChild[0]; // Right for reverse, left for forward
        }
    }

public:
    explicit BinaryTreeIterator(Node* root) : Parent(nullptr) {
        pushPath(root);
        Parent::m_ptr = m_path.empty() ? nullptr : m_path.top();
    }

    BinaryTreeIterator& operator++() {
        if (m_path.empty()) return *this;
        Node* current = m_path.top();
        m_path.pop();
        pushPath(current->m_pChild[Reverse ? 0 : 1]);
        Parent::m_ptr = m_path.empty() ? nullptr : m_path.top();
        return *this;
    }
};

#endif // CONTAINERS_TREEITERATOR_H
