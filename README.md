# 2026-II-CS-UNI-CC0E5-AEDAv

## 📦 Contenedores genéricos en C++23

Implementación de **contenedores de datos genéricos** en **C++23**:

- `Vector`
- Listas enlazadas: `LinkedList`, `DoubleLinkedList`, `CircularLinkedList`, `CircularDoubleLinkedList`
- `BinaryTree`
- `Heap`, con sus variantes `MinHeap` y `MaxHeap`

El diseño se sustenta en tres bloques:

1. **Traits**: describen el nodo, los iteradores y el comparador de cada contenedor.
2. **Iteradores**: implementados con el patrón **CRTP** (`GeneralIterator`).
3. **Funciones genéricas de recorrido**: `call`, `ApplyFunction`, `FirstThat`, `rcall`.

Además se incorpora sincronización con `mutex` y persistencia con `operator<<` / `operator>>`.

## 🧩 1. Jerarquía de clases de los Traits

Los contenedores genéricos se parametrizan con un struct de **traits** que define el tipo de valor (`value_type`), el nodo (`Node`), los iteradores (`ForwardIterator` / `BackwardIterator`) y el comparador (`Compare`).

```mermaid
classDiagram
    direction TB
    class DefaultTraits~T~ {
        +T value_type
        +Compare compare
    }
    class AscendingTraits~T~ {
        +T value_type
        +Compare compare
    }
    class DescendingTraits~T~ {
        +T value_type
        +Compare compare
    }
    DefaultTraits <|-- AscendingTraits : Ascendente
    DefaultTraits <|-- DescendingTraits : Descendente
    class VectorAscTraits~T~ {
        +T value_type
    }
    class LinkedListAscTraits~T~ {
        +T value_type
        +LinkedListNode~T~ Node
        +LinkedListForwardIterator~T~ ForwardIterator
    }
    class DoubleLinkedListAscTraits~T~ {
        +LinkedListBackwardIterator~T~ BackwardIterator
    }
    class BinaryTreeAscTraits~T~ {
        +BinaryTreeNode~T~ Node
        +BinaryTreeForwardIterator~T~ ForwardIterator
        +BinaryTreeBackwardIterator~T~ BackwardIterator
    }
    class HeapNodeTraits~T~ {
        +GeneralNode~T~ Node
        +VectorForwardIterator~T~ ForwardIterator
        +VectorBackwardIterator~T~ BackwardIterator
    }
    class HeapAscTraits~T~ {
        +T value_type
    }
    class HeapDescTraits~T~ {
        +T value_type
    }
    class LinkedListDescTraits~T~ {
        +T value_type
    }
    class DoubleLinkedListDescTraits~T~ {
        +T value_type
    }
    class BinaryTreeDescTraits~T~ {
        +T value_type
    }
    AscendingTraits <|-- LinkedListAscTraits : Ascendente
    LinkedListAscTraits <|-- DoubleLinkedListAscTraits : Ascendente
    DescendingTraits <|-- LinkedListDescTraits : Descendente
    LinkedListDescTraits <|-- DoubleLinkedListDescTraits : Descendente
    AscendingTraits <|-- BinaryTreeAscTraits : Ascendente
    DescendingTraits <|-- BinaryTreeDescTraits : Descendente
    AscendingTraits <|-- HeapAscTraits : Ascendente
    DescendingTraits <|-- HeapDescTraits : Descendente
    HeapNodeTraits <|-- HeapAscTraits : Interno
    HeapNodeTraits <|-- HeapDescTraits : Interno
```

## 🔁 2. Jerarquía de clases de los Iteradores

Los iteradores concretos heredan de `GeneralIterator`, un template basado en **CRTP** que mantiene la posición actual (`m_ptr`) y el inicio (`m_pStart`).

```mermaid
classDiagram
    direction TB
    class GeneralIterator~Derived~ {
        #T m_ptr
        #T m_pStart
    }
    class VectorForwardIterator~T~ {
    }
    class VectorBackwardIterator~T~ {
    }
    class LinkedListForwardIterator~T~ {
    }
    class LinkedListBackwardIterator~T~ {
    }
    class BinaryTreeIterator~T~ {
    }
    class BinaryTreeForwardIterator~T~ {
    }
    class BinaryTreeBackwardIterator~T~ {
    }
    GeneralIterator <|-- VectorForwardIterator : CRTP
    GeneralIterator <|-- VectorBackwardIterator : CRTP
    GeneralIterator <|-- LinkedListForwardIterator : CRTP
    GeneralIterator <|-- LinkedListBackwardIterator : CRTP
    GeneralIterator <|-- BinaryTreeIterator : CRTP
    BinaryTreeIterator <|-- BinaryTreeForwardIterator : Rama 0
    BinaryTreeIterator <|-- BinaryTreeBackwardIterator : Rama 1
```

## 📚 3. Jerarquía de clases de los Contenedores

Los contenedores se apoyan en *mixins* de herencia virtual sobre la base `Callable`, e implementan sus propios métodos de recorrido.

```mermaid
classDiagram
    direction TB
    class Callable~Derived~ {
        #T m_mutex
        +begin()
        +end()
        +call()
    }
    class Applicable~Derived~ {
        +apply()
    }
    class Firstable~Derived~ {
        +find()
    }
    class Recallable~Derived~ {
        +rcall()
    }
    Callable <|-- Applicable
    Callable <|-- Firstable
    Callable <|-- Recallable
    class Vector~Traits~ {
        +begin()
        +end()
        +apply()
        +find()
        +call()
        +rcall()
    }
    class LinkedList~Traits~ {
        +begin()
        +end()
        +apply()
        +find()
        +call()
    }
    class BinaryTree~Traits~ {
        +begin()
        +end()
        +find()
        +rcall()
    }
    class Heap~Traits~ {
        +apply()
    }
    class MinHeap~T~ {
        +Heap m_data
    }
    class MaxHeap~T~ {
        +Heap m_data
    }
    class CircularLinkedList~Traits~ {
        +insert()
    }
    class DoubleLinkedList~Traits~ {
        +insert()
        +rbegin()
        +rend()
    }
    class CircularDoubleLinkedList~Traits~ {
        +insert()
    }
    Applicable <|-- Heap
    Heap <|-- MinHeap
    Heap <|-- MaxHeap
    Firstable <|-- BinaryTree
    Recallable <|-- BinaryTree
    LinkedList <|-- CircularLinkedList
    LinkedList <|-- DoubleLinkedList
    CircularLinkedList <|-- CircularDoubleLinkedList
    DoubleLinkedList <|-- CircularDoubleLinkedList
```

## 🧩 4. Jerarquía de clases de los Nodos

Los contenedores trabajan sobre nodos genéricos que agrupan el valor (`m_value`) con una referencia (`m_ref`) al propietario del dato.

```mermaid
classDiagram
    direction TB
    class GeneralNode~T~ {
        -T m_value
        -Ref m_ref
        +getValue()
        +getRef()
        +value()
    }
    class LinkedListNode~T~ {
        +LinkedListNode~T~ m_pNext
        +LinkedListNode~T~ m_pPrev
    }
    class BinaryTreeNode~T~ {
        +BinaryTreeNode~T~ m_pChild
        +BinaryTreeNode~T~ m_pParent
    }
    GeneralNode <|-- LinkedListNode : hereda
    GeneralNode <|-- BinaryTreeNode : hereda
```

- `GeneralNode<T>`: nodo base que guarda el valor (`m_value`) y la referencia (`m_ref`).
- `LinkedListNode<T>`: agrega los enlaces `m_pNext` / `m_pPrev` para el nodo enlazado.
- `BinaryTreeNode<T>`: agrega los hijos (`m_pChild`) y el padre (`m_pParent`) para el árbol binario.

## 🛠️ Compilar y ejecutar

```bash
make
make run
```
