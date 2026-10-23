# 2026-II-CS-UNI-CC0E5-AEDAv

classDiagram
    direction TB

    class Callable~Derived,Traits~ {
        <<abstract>>
        #mutex m_mutex
        -begin()* ForwardIterator
        -end()*   ForwardIterator
        +call(Func, Args...) decltype(auto)
    }

    class Applicable~Derived,Traits~ {
        +ApplyFunction(Func, Args...)
    }

    class Firstable~Derived,Traits~ {
        +FirstThat(Func, Args...) Node&
    }

    class Heap~Traits~ {
        -Vector~Traits~ m_data
        -ForwardIterator begin()
        -ForwardIterator end()
        +push(value, ref)
        +pop() Node
        +clear()
        +HeapifyUp(index)
        +HeapifyDown(index)
        +write(os) ostream&
        +read(is)  istream&
    }

    class MinHeap~T,Traits~ 
    class MaxHeap~T,Traits~

    Callable <|-- Applicable
    Callable <|-- Firstable
    Applicable <|-- Heap
    Heap <|-- MinHeap
    Heap <|-- MaxHeap





    direction TB

    class HeapDefaultTraits~T,_Compare~ {
        +Node = GeneralNode~T~
        +value_type = T
        +Compare = _Compare
        +ForwardIterator = VectorForwardIterator~Node~
        +BackwardIterator = VectorBackwardIterator~Node~
    }

    class HeapAscTraits~T,less~ 
    class HeapDescTraits~T,greater~ 

    HeapDefaultTraits <|-- HeapAscTraits
    HeapDefaultTraits <|-- HeapDescTraits

    class Heap~Traits~
    class MinHeap~T,Traits~
    class MaxHeap~T,Traits~

    HeapAscTraits  ..> MinHeap : Traits por defecto
    HeapDescTraits ..> MaxHeap : Traits por defecto
    Heap ..> HeapDefaultTraits : Traits requerido
