#ifndef __CIRCULARLIST_H__
#define __CIRCULARLIST_H__
#include "linkedlist.h"

template <typename T>
class CircularForwardIterator : public GeneralIterator<CircularForwardIterator<T>, LinkedListNode<T>> {
    using NodePtr = LinkedListNode<T> *;
    NodePtr m_start;
public:
    using value_type        = LinkedListNode<T>;
    using MySelf            = CircularForwardIterator<T>;
    using Parent            = GeneralIterator<MySelf, value_type>;

    CircularForwardIterator(NodePtr ptr) : Parent(ptr), m_start(ptr) {}

    CircularForwardIterator& operator++() {
        Parent::m_ptr = Parent::m_ptr->m_pNext;
        if (Parent::m_ptr == m_start)
            Parent::m_ptr = nullptr;
        return *this;
    }
};

template <typename T>
struct CircularTraits : public LinkedListAscTraits<T> {
    using ForwardIterator   = CircularForwardIterator<T>;
};

template <typename T>
class LC : public LinkedList<CircularTraits<T>> {
    using Base = LinkedList<CircularTraits<T>>;
public:
    void push_back(const T& value, Ref ref) {
        Base::push_back(value, ref);
        lock_guard<mutex> lock(this->m_mutex);
        if (this->m_pTail)
            this->m_pTail->m_pNext = this->m_pRoot;
    }

    void insert(const T& value, Ref ref) {
        if (this->m_pTail)
            this->m_pTail->m_pNext = nullptr;
        Base::insert(value, ref);
        lock_guard<mutex> lock(this->m_mutex);
        auto current = this->m_pRoot;
        while (current->m_pNext != nullptr)
            current = current->m_pNext;
        current->m_pNext = this->m_pRoot;
        this->m_pTail = current;
    }
};

#endif // __CIRCULARLIST_H__