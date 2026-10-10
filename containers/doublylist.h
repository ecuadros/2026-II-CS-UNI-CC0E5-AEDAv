#ifndef __DOUBLYLIST_H__
#define __DOUBLYLIST_H__
#include "linkedlist.h"

template <typename T>
class DoubleLinkedListNode : public LinkedListNode<T> {
    using Node    = DoubleLinkedListNode<T>;
    using NodePtr = Node *;
    DoubleLinkedListNode *m_pPrev = nullptr; // puntero al nodo anterior
public:
    DoubleLinkedListNode(const T& value, Ref ref, LinkedListNode<T> *pNext, DoubleLinkedListNode *pPrev = nullptr)
        : LinkedListNode<T>(value, ref, pNext), m_pPrev(pPrev) {}
    DoubleLinkedListNode *GetPrev() const { return m_pPrev; }
    void SetPrev(DoubleLinkedListNode *pPrev) { m_pPrev = pPrev; }
};

template <typename T>
class DoubleLinkedListForwardIterator : public GeneralIterator<DoubleLinkedListForwardIterator<T>, DoubleLinkedListNode<T>> {
    using MySelf = DoubleLinkedListForwardIterator<T>;
    using Parent = GeneralIterator<MySelf, DoubleLinkedListNode<T>>;
public:
    using Parent::Parent;
    DoubleLinkedListForwardIterator& operator++() {
        Parent::m_ptr = static_cast<DoubleLinkedListNode<T> *>(Parent::m_ptr->m_pNext);
        return *this;
    }
};

template <typename T>
class DoubleLinkedListBackwardIterator : public GeneralIterator<DoubleLinkedListBackwardIterator<T>, DoubleLinkedListNode<T>> {
    using MySelf = DoubleLinkedListBackwardIterator<T>;
    using Parent = GeneralIterator<MySelf, DoubleLinkedListNode<T>>;
public:
    using Parent::Parent;
    DoubleLinkedListBackwardIterator& operator++() {
        Parent::m_ptr = Parent::m_ptr->GetPrev();
        return *this;
    }
};

template <typename T>
struct DoubleLinkedListTraits : public LinkedListAscTraits<T> {
    using Node              = DoubleLinkedListNode<T>;
    using ForwardIterator   = DoubleLinkedListForwardIterator<T>;
    using BackwardIterator  = DoubleLinkedListBackwardIterator<T>;
};

template <typename T>
class DoubleLinkedList : public LinkedList<DoubleLinkedListTraits<T>> {
    using Base    = LinkedList<DoubleLinkedListTraits<T>>;
    using value_type = typename Base::value_type;
    using Node    = typename Base::Node;
    using NodePtr = typename Base::NodePtr;
    using BackwardIterator = DoubleLinkedListBackwardIterator<T>;
public:
    DoubleLinkedList() {}

    void push_back(const value_type& value, Ref ref) {
        NodePtr pOldTail = this->m_pTail;
        Base::push_back(value, ref);
        this->m_pTail->SetPrev(pOldTail);
    }

    void insert(const value_type& value, Ref ref) {
        Base::insert(value, ref);
        NodePtr pPrev = nullptr;
        NodePtr current = this->m_pRoot;
        while (current != nullptr) {
            current->SetPrev(pPrev);
            pPrev = current;
            current = static_cast<NodePtr>(current->m_pNext);
        }
        this->m_pTail = pPrev;
    }

    BackwardIterator rbegin() { return BackwardIterator(this->m_pTail); }
    BackwardIterator rend()   { return BackwardIterator(nullptr); }
};

#endif // __DOUBLYLIST_H__
