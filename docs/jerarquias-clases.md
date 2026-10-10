# Jerarquías de clases del curso

> Diagramas de la evolución completa: `Vector` → `LinkedList` → lista circular (`LC`) → lista doblemente enlazada (`DoubleLinkedList`).
> Fuente: `containers/*.h`, `foreach.h`.

## 1 · Nodos

```mermaid
classDiagram
    class GeneralNode~T~ {
        -T m_value
        -Ref m_ref
        +getValue() T
        +getRef() Ref
        +value() T&
        +operator<<(ostream&, const GeneralNode~T~&)
    }
    class LinkedListNode~T~ {
        +Node* m_pNext
        +LinkedListNode(const T&, Ref, Node*)
    }
    class DoubleLinkedListNode~T~ {
        -DLLNode* m_pPrev
        +GetPrev() DLLNode*
        +SetPrev(DLLNode*)
        +DLLNode(const T&, Ref, LinkedListNode~T~*, DLLNode*)
    }
    GeneralNode~T~ <|-- LinkedListNode~T~ : +m_pNext
    LinkedListNode~T~ <|-- DoubleLinkedListNode~T~ : +m_pPrev
```

- `GeneralNode`: el par `(value, ref)` — la unidad mínima. `Vector` lo usa **directamente** (`VectorAscTraits::Node = GeneralNode<T>`).
- `LinkedListNode`: agrega el enlace hacia adelante (`m_pNext` público).
- `DoubleLinkedListNode`: agrega el enlace hacia atrás (`m_pPrev` privado con `GetPrev/SetPrev`).

## 2 · Iteradores (CRTP)

```mermaid
classDiagram
    class GeneralIterator~Derived,T~ {
        #pointer m_ptr
        +GeneralIterator(pointer)
        +operator*() value_type&
        +operator==(const Derived&) bool
        +operator!=(const Derived&) bool
    }
    class VectorForwardIterator~T~ {
        +operator++() : avanza m_ptr
    }
    class VectorBackwardIterator~T~ {
        +operator++() : retrocede m_ptr (rbegin/rend)
    }
    class LinkedListForwardIterator~T~ {
        +operator++() : m_ptr = m_ptr->m_pNext
    }
    class CircularForwardIterator~T~ {
        -NodePtr m_start
        +operator++() : avanza, se anula al volver a m_start
    }
    class DoubleLinkedListForwardIterator~T~ {
        +operator++() : avanza via m_pNext
    }
    class DoubleLinkedListBackwardIterator~T~ {
        +operator++() : retrocede via GetPrev()
    }
    GeneralIterator~Derived,T~ <|-- VectorForwardIterator~T~
    GeneralIterator~Derived,T~ <|-- VectorBackwardIterator~T~
    GeneralIterator~Derived,T~ <|-- LinkedListForwardIterator~T~
    GeneralIterator~Derived,T~ <|-- CircularForwardIterator~T~
    GeneralIterator~Derived,T~ <|-- DoubleLinkedListForwardIterator~T~
    GeneralIterator~Derived,T~ <|-- DoubleLinkedListBackwardIterator~T~
```

- El CRTP (`GeneralIterator<Derived, T>`) comparte `operator*` y los `==/!=` sobre el tipo concreto del hijo — polimorfismo estático, sin vptr (nota 04a).
- Todos exponen el mismo protocolo (`*`, `++`, `!=`) → los algoritmos genéricos de `foreach.h` los consumen sin saber qué contenedor es.
- `CircularForwardIterator` guarda `m_start` y se anula al completar la vuelta: es lo que hace alcanzable el `end()` en un anillo.

## 3 · Contenedores

```mermaid
classDiagram
    class Vector~Traits~ {
        -Node* m_data
        -size_t m_size, m_capacity
        -mutex m_mutex
        +push_back(const T&, Ref)
        +push_back(const T&, Ref, Ref)
        +insert()
        +write() ostream&
        +read() istream&
        +call() / rcall() / FirstThat() / ApplyFunction()
    }
    class LinkedList~Traits~ {
        #NodePtr m_pRoot, m_pTail
        #Compare m_comp
        #mutable mutex m_mutex
        +push_back() virtual
        +insert() : internalInsert recursivo (template)
        +clear() virtual
        +operator<</operator>> (persistencia)
        +call() / FirstThat() / ApplyFunction()
        +GetRoot()
    }
    class LC~T~ {
        +push_front() / pop_front() / pop_back()
        +front() / back()
        +clear() override (circle-safe)
    }
    class DoubleLinkedList~T~ {
        +push_back() : base + prev O(1)
        +insert() : base + repair walk
        +rbegin() / rend()
    }
    LinkedList~Traits~ <|-- LC~T~ : LinkedList~CircularTraits~T~~
    LinkedList~Traits~ <|-- DoubleLinkedList~T~ : LinkedList~DoubleLinkedListTraits~T~~
```

- `Vector` es la rama independiente (arreglo dinámico con `resize` + `std::exchange` en move).
- `LinkedList` es la base de las enlazadas: nodos encadenados, insert ordenado (recursión de cola con `NodePtrT&`), persistencia y algoritmos heredables.
- `LC` (lista circular): hereda y cierra el anillo — ops de deque (`push_front/pop_*`) en O(1) salvo `pop_back`.
- `DoubleLinkedList`: reutiliza el `insert` de la base y repara los `m_pPrev` con un walk; `rbegin/rend` recorren al revés.

## 4 · Traits

```mermaid
classDiagram
    class DefaultTraits~T,Compare~ {
        +value_type = T
        +Compare
    }
    class AscendingTraits~T~ {
        +Compare = std::less~T~
    }
    class DescendingTraits~T~ {
        +Compare = std::greater~T~
    }
    class LinkedListAscTraits~T~ {
        +Node = LinkedListNode~T~
        +ForwardIterator = LinkedListForwardIterator~T~
    }
    class LinkedListDescTraits~T~ {
        +Node = LinkedListNode~T~
        +ForwardIterator = LinkedListForwardIterator~T~
    }
    class CircularTraits~T~ {
        +ForwardIterator = CircularForwardIterator~T~
    }
    class DoubleLinkedListTraits~T~ {
        +Node = DoubleLinkedListNode~T~
        +ForwardIterator = DoubleLinkedListForwardIterator~T~
        +BackwardIterator = DoubleLinkedListBackwardIterator~T~
    }
    class VectorAscTraits~T~ {
        +Node = GeneralNode~T~
        +ForwardIterator = VectorForwardIterator~T~
        +BackwardIterator = VectorBackwardIterator~T~
    }
    DefaultTraits~T,Compare~ <|-- AscendingTraits~T~
    DefaultTraits~T,Compare~ <|-- DescendingTraits~T~
    AscendingTraits~T~ <|-- LinkedListAscTraits~T~
    DescendingTraits~T~ <|-- LinkedListDescTraits~T~
    LinkedListAscTraits~T~ <|-- CircularTraits~T~
    LinkedListAscTraits~T~ <|-- DoubleLinkedListTraits~T~
```

- Los traits son la "tabla de configuración" de cada contenedor: qué `Node` guarda, qué iteradores entrega, cómo ordena (`less/greater`).
- Cambiar el comportamiento del contenedor = otro traits, cero cambios en el contenedor (la lección de la nota 04a: `begin()/end()` devuelven `Traits::ForwardIterator`).
- `VectorAscTraits` vive en `vector.h` (rama de Vector); el resto en `linkedlist.h` / `circularlist.h` / `doublylist.h`.

## 5 · Composición y reuso

```mermaid
classDiagram
    class Vector~VectorAscTraits~ {
        usa: GeneralNode
        usa: VectorForward/BackwardIterator
    }
    class LinkedList~LinkedListAscTraits~ {
        usa: LinkedListNode
        usa: LinkedListForwardIterator
    }
    class LC~CircularTraits~ {
        hereda de: LinkedList
        usa: CircularForwardIterator
        reutiliza: insert de LinkedList (abrir anillo → base → recerrar)
    }
    class DoubleLinkedList~DoubleLinkedListTraits~ {
        hereda de: LinkedList
        usa: DoubleLinkedListNode + Fwd/Backward iteradores
        reutiliza: insert de LinkedList (+ repair walk de m_pPrev)
    }
    class foreach_h {
        ::call(begin, end, func, args...)
        ::ApplyFunction(begin, end, ...)
        ::FirstThat(begin, end, ...)
    }
    Vector~VectorAscTraits~ *-- GeneralNode~T~ : almacena
    LinkedList~LinkedListAscTraits~ *-- LinkedListNode~T~ : almacena
    LC~CircularTraits~ ..|> LinkedList~LinkedListAscTraits~
    DoubleLinkedList~DoubleLinkedListTraits~ ..|> LinkedList~LinkedListAscTraits~
    Vector~VectorAscTraits~ ..> foreach_h : call/rcall/FirstThat
    LinkedList~LinkedListAscTraits~ ..> foreach_h : call/FirstThat/ApplyFunction
    LC~CircularTraits~ ..> foreach_h : via begin()/end() heredados
    DoubleLinkedList~DoubleLinkedListTraits~ ..> foreach_h : via rbegin()/rend()
```

- Un solo `::call` genérico (foreach.h) sirve a **todos** los contenedores porque todos entregan pares de iteradores con el mismo protocolo.
- `LC` reutiliza el `insert` de la base abriendo y recerrando el anillo; `DoubleLinkedList` reutiliza el mismo `insert` (templado en `NodePtrT`) y repara los `m_pPrev` con un walk O(n) — la recursión es una sola para las tres listas.
- La persistencia (`operator<<` / `operator>>`) también es heredada: `>>` llama `clear()` y `push_back()` **virtuales**, por eso cada derivada mantiene su invariante (anillo cerrado, prevs correctos) sin duplicar el parser.
