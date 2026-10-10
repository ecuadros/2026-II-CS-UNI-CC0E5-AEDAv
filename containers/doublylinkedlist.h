#ifndef __DOUBLYLINKEDLIST_H__
#define __DOUBLYLINKEDLIST_H__
#include "linkedlist.h"

template <typename T>
class DoublyLinkedListNode : public LinkedListNode<T, DoublyLinkedListNode<T>> {
    using Parent = LinkedListNode<T, DoublyLinkedListNode<T>>;
public:
    DoublyLinkedListNode* m_pPrev = nullptr;
    DoublyLinkedListNode() : Parent(), m_pPrev(nullptr) {}
    DoublyLinkedListNode(const T& value, Ref ref, DoublyLinkedListNode* pNext, DoublyLinkedListNode* pPrev = nullptr)
        : Parent(value, ref, pNext), m_pPrev(pPrev) {}
};

template <typename Node>
class DoublyLinkedListBackwardIterator : public GeneralIterator<DoublyLinkedListBackwardIterator<Node>, Node> {
public:
    using value_type = Node;
    using MySelf = DoublyLinkedListBackwardIterator<Node>;
    using Parent = GeneralIterator<MySelf, value_type>;
    using Parent::Parent;
    DoublyLinkedListBackwardIterator& operator++() { Parent::m_ptr = Parent::m_ptr->m_pPrev; return *this; }
};

template <typename T>
struct DoublyLinkedListStructure {
    using Node              = DoublyLinkedListNode<T>;
    using ForwardIterator   = LinkedListForwardIterator<Node>;
    using BackwardIterator  = DoublyLinkedListBackwardIterator<Node>;
    static constexpr bool circular = false;
    static constexpr bool doubly   = true;
};

template <typename T>
using DoublyLinkedListAscTraits = ListTraits<AscendingTraits<T>, DoublyLinkedListStructure<T>>;

template <typename T>
using DoublyLinkedListDescTraits = ListTraits<DescendingTraits<T>, DoublyLinkedListStructure<T>>;

template <typename Traits>
class DoublyLinkedList : public LinkedList<Traits> {
    static_assert(Traits::doubly, "DoublyLinkedList requiere Traits con nodos doblemente enlazados");
    using Base = LinkedList<Traits>;
public:
    using value_type        = typename Base::value_type;
    using Node              = typename Base::Node;
    using NodePtr           = typename Base::NodePtr;
    using BackwardIterator  = typename Traits::BackwardIterator;
protected:
    void internalLink(NodePtr pred, NodePtr node, NodePtr succ) override {
        Base::internalLink(pred, node, succ);
        node->m_pPrev = pred;
        if (succ)
            succ->m_pPrev = node;
    }
public:
    DoublyLinkedList() {}
    DoublyLinkedList(const DoublyLinkedList& another) : Base() { this->copyFrom(another); }
    DoublyLinkedList& operator=(const DoublyLinkedList& another) = default;
    DoublyLinkedList(initializer_list<pair<value_type, Ref>> values) { this->append(values); }

    BackwardIterator rbegin() const { return BackwardIterator(this->GetTail()); }
    BackwardIterator rend() const { return BackwardIterator(nullptr); }

    template<typename Func, typename... Args>
    decltype(auto) rcall(Func func, Args&&... args)
    {    auto lock = this->Lock();
        return ::call(rbegin(), rend(), std::move(func), std::forward<Args>(args)...);
    }
};

#endif
