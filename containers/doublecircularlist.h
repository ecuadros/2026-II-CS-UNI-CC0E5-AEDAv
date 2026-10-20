#ifndef __DOUBLECIRCULARLIST_H__
#define __DOUBLECIRCULARLIST_H__
#include "doublylist.h"

template <typename T>
class DoubleCircularForwardIterator : public GeneralIterator<DoubleCircularForwardIterator<T>, LinkedListNode<T>> {
    using NodePtr = LinkedListNode<T> *;
    NodePtr m_start;
public:
    using value_type        = LinkedListNode<T>;
    using MySelf            = DoubleCircularForwardIterator<T>;
    using Parent            = GeneralIterator<MySelf, value_type>;

    DoubleCircularForwardIterator(NodePtr ptr) : Parent(ptr), m_start(ptr) {}

    DoubleCircularForwardIterator& operator++() {
        Parent::m_ptr = Parent::m_ptr->m_pNext;
        if (Parent::m_ptr == m_start)
            Parent::m_ptr = nullptr;
        return *this;
    }
};

template <typename T>
class DoubleCircularBackwardIterator : public GeneralIterator<DoubleCircularBackwardIterator<T>, DoubleLinkedListNode<T>> {
    using NodePtr = DoubleLinkedListNode<T> *;
    NodePtr m_start;
public:
    using value_type        = DoubleLinkedListNode<T>;
    using MySelf            = DoubleCircularBackwardIterator<T>;
    using Parent            = GeneralIterator<MySelf, value_type>;

    DoubleCircularBackwardIterator(NodePtr ptr) : Parent(ptr), m_start(ptr) {}

    DoubleCircularBackwardIterator& operator++() {
        Parent::m_ptr = Parent::m_ptr->GetPrev();
        if (Parent::m_ptr == m_start)
            Parent::m_ptr = nullptr;
        return *this;
    }
};

template <typename T>
struct DoubleCircularTraits : public DoubleLinkedListTraits<T> {
    using ForwardIterator   = DoubleCircularForwardIterator<T>;
    using BackwardIterator  = DoubleCircularBackwardIterator<T>;
};

template <typename T>
class DoubleCircularLinkedList : public DoubleLinkedList<DoubleCircularTraits<T>> {
    using Base = DoubleLinkedList<DoubleCircularTraits<T>>;
public:
    void push_back(const T& value, Ref ref) {
        Base::push_back(value, ref);
        lock_guard<mutex> lock(this->m_mutex);
        if (this->m_pTail) {
            this->m_pTail->m_pNext = this->m_pRoot;
            this->m_pRoot->SetPrev(this->m_pTail);
        }
    }

    void insert(const T& value, Ref ref) {
        if (this->m_pTail)
            this->m_pTail->m_pNext = nullptr;
        Base::insert(value, ref);
        this->m_pTail->m_pNext = this->m_pRoot;
        this->m_pRoot->SetPrev(this->m_pTail);
    }
};

#endif // __DOUBLECIRCULARLIST_H__