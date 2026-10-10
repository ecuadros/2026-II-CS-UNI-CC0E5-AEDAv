#ifndef __CIRCULARLINKEDLIST_H__
#define __CIRCULARLINKEDLIST_H__
#include "linkedlist.h"

template <typename Node>
class CircularLinkedListForwardIterator : public GeneralIterator<CircularLinkedListForwardIterator<Node>, Node> {
public:
    using value_type = Node;
    using MySelf = CircularLinkedListForwardIterator<Node>;
    using Parent = GeneralIterator<MySelf, value_type>;
private:
    Node* m_pStart;
public:
    CircularLinkedListForwardIterator(Node* ptr) : Parent(ptr), m_pStart(ptr) {}
    CircularLinkedListForwardIterator& operator++() {
        Parent::m_ptr = Parent::m_ptr->m_pNext;
        if (Parent::m_ptr == m_pStart)
            Parent::m_ptr = nullptr;
        return *this;
    }
};

template <typename T>
struct CircularLinkedListStructure {
    using Node              = LinkedListNode<T>;
    using ForwardIterator   = CircularLinkedListForwardIterator<Node>;
    static constexpr bool circular = true;
    static constexpr bool doubly   = false;
};

template <typename T>
using CircularLinkedListAscTraits = ListTraits<AscendingTraits<T>, CircularLinkedListStructure<T>>;

template <typename T>
using CircularLinkedListDescTraits = ListTraits<DescendingTraits<T>, CircularLinkedListStructure<T>>;

template <typename Traits>
class CircularLinkedList : public LinkedList<Traits> {
    static_assert(Traits::circular, "CircularLinkedList requiere Traits con estructura circular");
    using Base = LinkedList<Traits>;
public:
    using value_type    = typename Base::value_type;
    using Node          = typename Base::Node;
    using NodePtr       = typename Base::NodePtr;
protected:
    void internalLink(NodePtr pred, NodePtr node, NodePtr succ) override {
        Base::internalLink(pred, node, succ);
        this->GetTail()->m_pNext = this->GetRoot();
    }
public:
    CircularLinkedList() {}
    CircularLinkedList(const CircularLinkedList& another) : Base() { this->copyFrom(another); }
    CircularLinkedList& operator=(const CircularLinkedList& another) = default;
    CircularLinkedList(initializer_list<pair<value_type, Ref>> values) { this->append(values); }
};

#endif
