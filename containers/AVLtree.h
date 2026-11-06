#ifndef __AVL_TREE_H__
#define __AVL_TREE_H__
#include "binarytree.h"

template <typename Traits>
class AVL_Tree: public BinaryTree<Traits> {
public:
    using MySelf = AVL_Tree;
    using Parent = BinaryTree<Traits>;
    using typename Parent::value_type;
    using typename Parent::Node;
    using typename Parent::NodePtr;
    using ForwardIterator   = typename Traits::ForwardIterator;
    using BackwardIterator = typename Traits::BackwardIterator;
    using Compare = typename Traits::Compare;
    using Order = typename Traits::Order;

    void rotate(NodePtr nodeptr, TX _Dir);
    TX balance(NodePtr nodeptr);
    void Redistribution(NodePtr nodeptr) override;
};

template <typename Traits>
void AVL_Tree<Traits>::rotate(NodePtr nodeptr, TX Dir) {
    auto newNodePtr = nodeptr->m_pChild[1-Dir];
    //if (!newNodePtr) return nodeptr;

    auto tmp = newNodePtr->m_pChild[Dir];

    newNodePtr->m_pChild[Dir] = nodeptr;
    nodeptr->m_pChild[1-Dir] = tmp;

    newNodePtr->m_parent = nodeptr->m_parent;
    nodeptr->m_parent = newNodePtr;

    if (tmp) tmp->m_parent = nodeptr;

    nodeptr->m_height = Parent::height(nodeptr);
    newNodePtr->m_height = Parent::height(newNodePtr);
}

template <typename Traits>
TX AVL_Tree<Traits>::balance(NodePtr nodeptr) {
    if (!nodeptr) return 0;
    return Parent::height(nodeptr->m_pChild[0]) - Parent::height(nodeptr->m_pChild[1]);
}

template <typename Traits>
void AVL_Tree<Traits>::Redistribution(NodePtr nodeptr) {
    cout << nodeptr->getValue() << "  " << balance(nodeptr) << "\n";
    if (std::abs(balance(nodeptr)) > 1) {
        auto childNodePtr = nodeptr->m_pChild[balance(nodeptr)<0];
        //cout << "AQUI05\n";
        if (balance(nodeptr) * balance(childNodePtr) < 0) {
            rotate(childNodePtr, balance(childNodePtr)<0);
        }
        rotate(nodeptr, balance(nodeptr)>0);
    }
}

#endif // __AVL_TREE_H__
