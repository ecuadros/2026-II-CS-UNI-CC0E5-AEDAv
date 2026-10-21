#ifndef __CIRCULARDOUBLYLINKEDLIST_H__
#define __CIRCULARDOUBLYLINKEDLIST_H__
#include "doublylinkedlist.h"

template <typename T>
struct CircularDoublyLinkedListStructure {
    using Node              = DoublyLinkedListNode<T>;
    using ForwardIterator   = LinkedListIterator<Node, true>;
    using BackwardIterator  = LinkedListIterator<Node, true, &Node::m_pPrev>;
    static constexpr bool circular = true;
    static constexpr bool doubly   = true;
};

template <typename T>
using CircularDoublyLinkedListAscTraits = ListTraits<AscendingTraits<T>, CircularDoublyLinkedListStructure<T>>;

template <typename T>
using CircularDoublyLinkedListDescTraits = ListTraits<DescendingTraits<T>, CircularDoublyLinkedListStructure<T>>;

template <typename Traits>
class CircularDoublyLinkedList : public DoublyLinkedList<Traits> {
    using Base = DoublyLinkedList<Traits>;
    using Base::Base;
};

#endif
