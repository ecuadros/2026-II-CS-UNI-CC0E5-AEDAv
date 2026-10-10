  #ifndef __DOUBLY_LINKED_LIST_NODE_H__
  #define __DOUBLY_LINKED_LIST_NODE_H__

  #include "linkedlist.h"

  template <typename Traits>
  class DoublyLinkedListNode : public LinkedListNode<Traits> {
  public:
      using value_type = typename Traits::value_type;
      using Node       = typename Traits::Node;
      using NodePtr    = Node*;

      Node* m_pPrev = nullptr;

      DoublyLinkedListNode() 
          : LinkedListNode<Traits>(), m_pPrev(nullptr) {}

      DoublyLinkedListNode(const value_type& value, Ref ref, Node* pNext = nullptr, Node* pPrev = nullptr)
          : LinkedListNode<Traits>(value, ref, pNext), m_pPrev(pPrev) {}
  };

  #endif // __DOUBLY_LINKED_LIST_NODE_H__