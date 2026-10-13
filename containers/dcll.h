#ifndef __DCLL_H__
#define __DCLL_H__

#include "dll.h"

template <typename Traits>
class CircularDoublyLinkedList: public DoublyLinkedList<Traits> {
    public:
        using value_type        = typename Traits::value_type;
        using Node              = typename Traits::Node;
        using NodePtr           = Node *;
        using ForwardIterator   = typename Traits::ForwardIterator;
        using BackwardIterator   = typename Traits::BackwardIterator;
        using Compare           = typename Traits::Compare;
        using Delim             = typename Node::Delim;


        ForwardIterator end() const override { return ForwardIterator(this->m_pRoot); }
        BackwardIterator rend() const { return BackwardIterator(this->m_pTail); }

        void push_back(const value_type& value, Ref ref) override {
            DoublyLinkedList<Traits>::push_back(value, ref);
            this->m_pTail->m_pNext = this->m_pRoot;
            this->m_pRoot->m_pPrev = this->m_pTail;
        }

        void internalInsert (const value_type& value, Ref ref, NodePtr& rParent) override {
            DoublyLinkedList<Traits>::internalInsert(value, ref, rParent);
            this->m_pTail->m_pNext = this->m_pRoot;
            this->m_pRoot->m_pPrev = this->m_pTail;
        }
};


#endif // __DCLL_H__
