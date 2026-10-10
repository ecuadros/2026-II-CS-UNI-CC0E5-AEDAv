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
    using Base    = LinkedList<CircularTraits<T>>;
    using value_type = typename Base::value_type;
    using Node    = typename Base::Node;
    using NodePtr = typename Base::NodePtr;
public:
    LC() {}

    void clear() {
        lock_guard<mutex> lock(this->m_mutex);
        if (this->m_pRoot == nullptr)
            return;
        NodePtr current = this->m_pRoot->m_pNext;
        while (current != this->m_pRoot) {
            NodePtr pNext = current->m_pNext;
            delete current;
            current = pNext;
        }
        delete this->m_pRoot;
        this->m_pRoot = nullptr;
        this->m_pTail = nullptr;
    }

    void push_back(const value_type& value, Ref ref) {
        Base::push_back(value, ref);
        lock_guard<mutex> lock(this->m_mutex);
        if (this->m_pTail)
            this->m_pTail->m_pNext = this->m_pRoot;
    }

    void push_front(const value_type& value, Ref ref) {
        lock_guard<mutex> lock(this->m_mutex);
        NodePtr pNew = new Node(value, ref, this->m_pRoot);
        if (this->m_pRoot == nullptr) {
            this->m_pRoot = pNew;
            this->m_pTail = pNew;
            pNew->m_pNext = pNew;
        } else {
            this->m_pTail->m_pNext = pNew;
            this->m_pRoot = pNew;
        }
    }

    void insert(const value_type& value, Ref ref) {
        if (this->m_pTail)
            this->m_pTail->m_pNext = nullptr;
        Base::insert(value, ref);
        lock_guard<mutex> lock(this->m_mutex);
        NodePtr current = this->m_pRoot;
        while (current->m_pNext != nullptr)
            current = current->m_pNext;
        current->m_pNext = this->m_pRoot;
        this->m_pTail = current;
    }

    void pop_front() {
        lock_guard<mutex> lock(this->m_mutex);
        if (this->m_pRoot == nullptr)
            return;
        NodePtr pOld = this->m_pRoot;
        if (pOld == this->m_pTail) {
            this->m_pRoot = nullptr;
            this->m_pTail = nullptr;
        } else {
            this->m_pTail->m_pNext = pOld->m_pNext;
            this->m_pRoot = pOld->m_pNext;
        }
        delete pOld;
    }

    void pop_back() {
        lock_guard<mutex> lock(this->m_mutex);
        if (this->m_pRoot == nullptr)
            return;
        if (this->m_pRoot == this->m_pTail) {
            delete this->m_pRoot;
            this->m_pRoot = nullptr;
            this->m_pTail = nullptr;
            return;
        }
        NodePtr current = this->m_pRoot;
        while (current->m_pNext != this->m_pTail)
            current = current->m_pNext;
        current->m_pNext = this->m_pRoot;
        delete this->m_pTail;
        this->m_pTail = current;
    }

    value_type& front() { return this->m_pRoot->value(); }
    value_type& back()  { return this->m_pTail->value(); }
};

#endif // __CIRCULARLIST_H__
