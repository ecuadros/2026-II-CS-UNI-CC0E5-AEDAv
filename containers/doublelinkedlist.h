#ifndef __DOUBLE_LINKEDLIST_H__
#define __DOUBLE_LINKEDLIST_H__

#include "linkedlist.h"

template <typename T>
class LinkedListBackwardIterator : public GeneralIterator<LinkedListBackwardIterator<T>, LinkedListNode<T>> {
public:
    using value_type = LinkedListNode<T>;
    using MySelf = LinkedListBackwardIterator<T>;
    using Parent = GeneralIterator<MySelf, value_type>;
    using Parent::Parent; // Inherit constructor
    LinkedListBackwardIterator& operator++() {
        Parent::m_ptr = Parent::m_ptr->m_pPrev == Parent::m_pStart ? nullptr : Parent::m_ptr->m_pPrev;
        return *this;
    }
};

template <typename T>
struct DoubleLinkedListAscTraits : public LinkedListAscTraits<T> {
    using BackwardIterator  = LinkedListBackwardIterator<T>;
};

template <typename T>
struct DoubleLinkedListDescTraits : public LinkedListDescTraits<T> {
    using BackwardIterator  = LinkedListBackwardIterator<T>;
};

template <typename Traits>
class DoubleLinkedList : virtual public LinkedList<Traits>, public ReversibleContainer<Traits> {
public:
    using Base             = LinkedList<Traits>;
    using BackwardIterator = typename Traits::BackwardIterator;
    using NodePtr          = typename Base::NodePtr;

protected:
    void linkNode(NodePtr pNew, NodePtr pPrev, NodePtr pNext = nullptr) override {
        pNew->m_pPrev = pPrev;
        if (pNext) pNext->m_pPrev = pNew;
    }

public:
    BackwardIterator rbegin() const override { return BackwardIterator(this->m_pTail); }
    BackwardIterator rend()   const override { return BackwardIterator(nullptr); }
};

#endif