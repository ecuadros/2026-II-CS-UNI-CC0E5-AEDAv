#ifndef __CIRCULARLINKEDLIST_H__
#define __CIRCULARLINKEDLIST_H__
#include "linkedlist.h"

template <typename T>
struct CircularLinkedListStructure {
    using Node              = LinkedListNode<T>;
    using ForwardIterator   = LinkedListIterator<Node, true>;
    static constexpr bool circular = true;
    static constexpr bool doubly   = false;
};

template <typename T>
using CircularLinkedListAscTraits = ListTraits<AscendingTraits<T>, CircularLinkedListStructure<T>>;

template <typename T>
using CircularLinkedListDescTraits = ListTraits<DescendingTraits<T>, CircularLinkedListStructure<T>>;

template <typename Traits>
class CircularLinkedList : public LinkedList<Traits> {
    using Base = LinkedList<Traits>;
    using Base::Base;
};

#endif
