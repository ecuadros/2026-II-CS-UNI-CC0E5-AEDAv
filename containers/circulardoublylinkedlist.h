#ifndef __CIRCULARDOUBLYLINKEDLIST_H__
#define __CIRCULARDOUBLYLINKEDLIST_H__
#include "circularlinkedlist.h"
#include "doublylinkedlist.h"

template <typename Node>
class CircularDoublyLinkedListBackwardIterator : public GeneralIterator<CircularDoublyLinkedListBackwardIterator<Node>, Node> {
public:
    using value_type = Node;
    using MySelf = CircularDoublyLinkedListBackwardIterator<Node>;
    using Parent = GeneralIterator<MySelf, value_type>;
private:
    Node* m_pStart;
public:
    CircularDoublyLinkedListBackwardIterator(Node* ptr) : Parent(ptr), m_pStart(ptr) {}
    CircularDoublyLinkedListBackwardIterator& operator++() {
        Parent::m_ptr = Parent::m_ptr->m_pPrev;
        if (Parent::m_ptr == m_pStart)
            Parent::m_ptr = nullptr;
        return *this;
    }
};

template <typename T>
struct CircularDoublyLinkedListStructure {
    using Node              = DoublyLinkedListNode<T>;
    using ForwardIterator   = CircularLinkedListForwardIterator<Node>;
    using BackwardIterator  = CircularDoublyLinkedListBackwardIterator<Node>;
    static constexpr bool circular = true;
    static constexpr bool doubly   = true;
};

template <typename T>
using CircularDoublyLinkedListAscTraits = ListTraits<AscendingTraits<T>, CircularDoublyLinkedListStructure<T>>;

template <typename T>
using CircularDoublyLinkedListDescTraits = ListTraits<DescendingTraits<T>, CircularDoublyLinkedListStructure<T>>;

template <typename Traits>
class CircularDoublyLinkedList : public DoublyLinkedList<Traits> {
    static_assert(Traits::circular, "CircularDoublyLinkedList requiere Traits con estructura circular");
    using Base = DoublyLinkedList<Traits>;
public:
    using value_type    = typename Base::value_type;
    using Node          = typename Base::Node;
    using NodePtr       = typename Base::NodePtr;
protected:
    void internalLink(NodePtr pred, NodePtr node, NodePtr succ) override {
        Base::internalLink(pred, node, succ);
        this->GetTail()->m_pNext = this->GetRoot();
        this->GetRoot()->m_pPrev = this->GetTail();
    }
public:
    CircularDoublyLinkedList() {}
    CircularDoublyLinkedList(const CircularDoublyLinkedList& another) : Base() { this->copyFrom(another); }
    CircularDoublyLinkedList& operator=(const CircularDoublyLinkedList& another) = default;
    CircularDoublyLinkedList(initializer_list<pair<value_type, Ref>> values) { this->append(values); }
};

#endif
