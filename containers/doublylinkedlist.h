#ifndef __DOUBLYLINKEDLIST_H__
#define __DOUBLYLINKEDLIST_H__
#include "linkedlist.h"

template <typename T>
class DoublyLinkedListNode : public LinkedListNode<T, DoublyLinkedListNode<T>> {
    using Parent = LinkedListNode<T, DoublyLinkedListNode<T>>;
public:
    DoublyLinkedListNode* m_pPrev = nullptr;
    using Parent::Parent;
};

template <typename T>
struct DoublyLinkedListStructure {
    using Node              = DoublyLinkedListNode<T>;
    using ForwardIterator   = LinkedListIterator<Node>;
    using BackwardIterator  = LinkedListIterator<Node, false, &Node::m_pPrev>;
    static constexpr bool circular = false;
    static constexpr bool doubly   = true;
};

template <typename T>
using DoublyLinkedListAscTraits = ListTraits<AscendingTraits<T>, DoublyLinkedListStructure<T>>;

template <typename T>
using DoublyLinkedListDescTraits = ListTraits<DescendingTraits<T>, DoublyLinkedListStructure<T>>;

template <typename Traits>
class DoublyLinkedList : public LinkedList<Traits, DoublyLinkedList<Traits>> {
    using Base = LinkedList<Traits, DoublyLinkedList<Traits>>;
    using Base::Base;
public:
    using BackwardIterator  = typename Traits::BackwardIterator;

    BackwardIterator rbegin() const { return BackwardIterator(this->GetTail()); }
    BackwardIterator rend() const { return BackwardIterator(nullptr); }
};

#endif
