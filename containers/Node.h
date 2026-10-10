#ifndef __NODE_H__
#define __NODE_H__
#include "GeneralNode.h"

template <typename T>
class LinkedListNode : public GeneralNode<T> {
    using Node = LinkedListNode<T>;
    using NodePtr = Node*;
public:
    Node* m_pNext = nullptr; // puntero al siguiente nodo, npara acceso directo necesita ser publico
    LinkedListNode() : GeneralNode<T>(T{}, Ref{}), m_pNext(nullptr) {}
    // añadir friend class añade otro typename al template
    LinkedListNode(const T& value, Ref ref, Node* pNext) : GeneralNode<T>(value, ref), m_pNext(pNext) {}
};

template <typename T>
class DoublyLinkedListNode : public GeneralNode<T> {
    using Node = DoublyLinkedListNode<T>;
    using NodePtr = Node*;
public:
    Node* m_pNext = nullptr; // puntero al siguiente nodo, npara acceso directo necesita ser publico
    Node* m_pPrev = nullptr;
    DoublyLinkedListNode() : GeneralNode<T>(T{}, Ref{}), m_pNext(nullptr), m_pPrev(nullptr) {}
    // añadir friend class añade otro typename al template
    DoublyLinkedListNode(const T& value, Ref ref, Node* pNext, Node* pPrev) : GeneralNode<T>(value, ref), m_pNext(pNext), m_pPrev(pPrev) {}
};

/*template <typename T>
class DoublyLinkedListNode : public LinkedListNode<T> {
    using Node = DoublyLinkedListNode<T>;
    using NodePtr = Node*;
public:
    Node* m_pPrev = nullptr;
    DoublyLinkedListNode() : LinkedListNode<T>(T{}, Ref{}, NodePtr{}), m_pPrev(nullptr) {}
    // añadir friend class añade otro typename al template
    DoublyLinkedListNode(const T& value, Ref ref, Node* pNext, Node* pPrev) : LinkedListNode<T>(value, ref, pNext), m_pPrev(pPrev) {}
    };*/

#endif // __NODE_H__
