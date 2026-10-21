#ifndef CONTAINERS_TREEITERATOR_H
#define CONTAINERS_TREEITERATOR_H

#include "GeneralIterator.h"
#include <stack>

enum class TreeTraversalOrder { PreOrder, InOrder, PostOrder };

template <typename Node, bool Reverse = false, TreeTraversalOrder Order = TreeTraversalOrder::InOrder>
class BinaryTreeIterator
    : public GeneralIterator<BinaryTreeIterator<Node, Reverse, Order>, Node> {
    using Parent = GeneralIterator<BinaryTreeIterator<Node, Reverse, Order>, Node>;

    struct Step {
        Node* node;
        bool visit;
    };

    std::stack<Step> m_pending;

    void advance() {
        Parent::m_ptr = nullptr;
        while (!m_pending.empty()) {
            auto [node, visit] = m_pending.top();
            m_pending.pop();
            if (visit) {
                Parent::m_ptr = node;
                return;
            }

            Node* first = node->m_pChild[Reverse ? 1 : 0];
            Node* second = node->m_pChild[Reverse ? 0 : 1];
            if constexpr (Order == TreeTraversalOrder::PreOrder) {
                if (second) m_pending.push({second, false});
                if (first) m_pending.push({first, false});
                m_pending.push({node, true});
            } else if constexpr (Order == TreeTraversalOrder::InOrder) {
                if (second) m_pending.push({second, false});
                m_pending.push({node, true});
                if (first) m_pending.push({first, false});
            } else {
                m_pending.push({node, true});
                if (second) m_pending.push({second, false});
                if (first) m_pending.push({first, false});
            }
        }
    }

public:
    explicit BinaryTreeIterator(Node* root) : Parent(nullptr) {
        if (root) m_pending.push({root, false});
        advance();
    }

    BinaryTreeIterator& operator++() {
        advance();
        return *this;
    }
};

#endif // CONTAINERS_TREEITERATOR_H
