# AEDAv — Biblioteca de Estructuras de Datos Avanzadas

Documentación técnica generada con **Doxygen** para el curso **CC0E5 (AEDAv) — Universidad Nacional de Ingeniería (UNI)**.

---

## Arquitectura General

La biblioteca está diseñada en **C++23** alrededor de un núcleo componible basado en **Traits**, **Mixins** y **Conceptos**:

- **Núcleo iterable y thread-safe:** `IterableCore` y `ContainerCore` encapsulan el protocolo de iteración y un `std::recursive_mutex` para operaciones compuestas seguras.
- **Composición por Mixins:** `GeneralContainer` ensambla capacidades modulares (`Callable`, `Searchable`, `Printable`, `Applicable`, `Reversible`, `Readable`, `Drawable`) definidas en `mixins.h`.
- **Vistas no propietarias $O(1)$:** `BaseView` y `FilterViewMixin` filtran en tiempo de compilación únicamente los mixins de consulta compatibles con vistas de solo lectura.
- **Iteradores CRTP y Recorrido de Euler:** `GeneralIterator` provee la base para iteradores lineales, mientras que `BinaryTreeIterator` unifica los 14 recorridos de árboles binarios (7 órdenes × 2 direcciones) mediante una máquina de estados por máscara de bits (`Phase`, `Order`, `Direction`).
- **Visualización 2D desacoplada:** `Drawable` proyecta cualquier contenedor mediante un layout (`BaseLayout`, `TreeLayout`, `VerticalTreeLayout`, `HorizontalTreeLayout`, `CircularListLayout`, `MerkleTreeLayout`) hacia un `VisualGraph` agnóstico, que `ConsoleGraphRenderer` dibuja en consola sobre `Canvas2D`.

---

## Mapa Arquitectónico Interactivo

Haz clic en cualquier clase del diagrama para navegar directamente a su documentación:

@dot
digraph ArchitectureMap {
    rankdir=TB;
    compound=true;
    fontname="Sans";
    node [shape=box, style="rounded,filled", fontname="Sans", fontsize=10, margin="0.14,0.08"];
    edge [fontname="Sans", fontsize=9, color="#475569"];

    subgraph cluster_core {
        label="Núcleo, Traits y Mixins";
        style="rounded,filled";
        color="#3b82f6";
        fillcolor="#eff6ff";
        fontname="Sans";

        IterableCore     [label="IterableCore", fillcolor="#dbeafe", color="#2563eb", URL="\ref IterableCore"];
        ContainerCore    [label="ContainerCore", fillcolor="#dbeafe", color="#2563eb", URL="\ref ContainerCore"];
        GeneralContainer [label="GeneralContainer", fillcolor="#bfdbfe", color="#1d4ed8", URL="\ref GeneralContainer"];
        BaseView         [label="BaseView", fillcolor="#dbeafe", color="#2563eb", URL="\ref BaseView"];
        GeneralIterator  [label="GeneralIterator", fillcolor="#dbeafe", color="#2563eb", URL="\ref GeneralIterator"];
        Drawable         [label="Drawable (Mixin)", fillcolor="#e0e7ff", color="#4f46e5", URL="\ref Drawable"];

        IterableCore -> ContainerCore;
        ContainerCore -> GeneralContainer;
        IterableCore -> BaseView;
        GeneralContainer -> Drawable [style=dashed, label="mixin"];
    }

    subgraph cluster_containers {
        label="Contenedores Lineales y Asociativos";
        style="rounded,filled";
        color="#10b981";
        fillcolor="#ecfdf5";
        fontname="Sans";

        BaseVector         [label="BaseVector", fillcolor="#d1fae5", color="#059669", URL="\ref BaseVector"];
        BaseLinkedList     [label="BaseLinkedList", fillcolor="#d1fae5", color="#059669", URL="\ref BaseLinkedList"];
        BaseDoubleLL       [label="BaseDoubleLinkedList", fillcolor="#d1fae5", color="#059669", URL="\ref BaseDoubleLinkedList"];
        BaseCircularLL     [label="BaseCircularLinkedList", fillcolor="#d1fae5", color="#059669", URL="\ref BaseCircularLinkedList"];
        BaseCircularDLL    [label="BaseCircularDoubleLinkedList", fillcolor="#a7f3d0", color="#047857", URL="\ref BaseCircularDoubleLinkedList"];
        BaseHashTable      [label="BaseHashTable", fillcolor="#d1fae5", color="#059669", URL="\ref BaseHashTable"];

        BaseLinkedList -> BaseDoubleLL;
        BaseLinkedList -> BaseCircularLL;
        BaseDoubleLL -> BaseCircularDLL;
        BaseCircularLL -> BaseCircularDLL;
    }

    subgraph cluster_trees {
        label="Árboles y Montículos";
        style="rounded,filled";
        color="#f59e0b";
        fillcolor="#fffbeb";
        fontname="Sans";

        BaseBinaryTree     [label="BaseBinaryTree", fillcolor="#fef3c7", color="#d97706", URL="\ref BaseBinaryTree"];
        BinaryTreeIterator [label="BinaryTreeIterator", fillcolor="#fef3c7", color="#d97706", URL="\ref BinaryTreeIterator"];
        BaseAVLTree        [label="BaseAVLTree", fillcolor="#fde68a", color="#b45309", URL="\ref BaseAVLTree"];
        BaseHeap           [label="BaseHeap", fillcolor="#fef3c7", color="#d97706", URL="\ref BaseHeap"];
        BaseMerkleTree     [label="BaseMerkleTree", fillcolor="#fde68a", color="#b45309", URL="\ref BaseMerkleTree"];

        BaseBinaryTree -> BaseAVLTree;
        BaseBinaryTree -> BaseMerkleTree;
        BaseBinaryTree -> BinaryTreeIterator [style=dotted, label="itera"];
    }

    subgraph cluster_viz {
        label="Layouts 2D y Renderizado";
        style="rounded,filled";
        color="#8b5cf6";
        fillcolor="#f5f3ff";
        fontname="Sans";

        BaseLayout           [label="BaseLayout", fillcolor="#ede9fe", color="#7c3aed", URL="\ref BaseLayout"];
        TreeLayout           [label="TreeLayout", fillcolor="#ede9fe", color="#7c3aed", URL="\ref TreeLayout"];
        VerticalTreeLayout   [label="VerticalTreeLayout", fillcolor="#ddd6fe", color="#6d28d9", URL="\ref VerticalTreeLayout"];
        HorizontalTreeLayout [label="HorizontalTreeLayout", fillcolor="#ddd6fe", color="#6d28d9", URL="\ref HorizontalTreeLayout"];
        CircularListLayout   [label="CircularListLayout", fillcolor="#ddd6fe", color="#6d28d9", URL="\ref CircularListLayout"];
        MerkleTreeLayout     [label="MerkleTreeLayout", fillcolor="#ddd6fe", color="#6d28d9", URL="\ref MerkleTreeLayout"];
        VisualGraph          [label="VisualGraph", fillcolor="#ede9fe", color="#7c3aed", URL="\ref VisualGraph"];
        GraphRenderer        [label="GraphRenderer", fillcolor="#ede9fe", color="#7c3aed", URL="\ref GraphRenderer"];
        ConsoleGraphRenderer [label="ConsoleGraphRenderer", fillcolor="#ddd6fe", color="#6d28d9", URL="\ref ConsoleGraphRenderer"];

        BaseLayout -> TreeLayout;
        BaseLayout -> CircularListLayout;
        BaseLayout -> MerkleTreeLayout;
        TreeLayout -> VerticalTreeLayout;
        TreeLayout -> HorizontalTreeLayout;
        BaseLayout -> VisualGraph [style=dashed, label="produce"];
        VisualGraph -> GraphRenderer [style=dashed, label="consume"];
        GraphRenderer -> ConsoleGraphRenderer;
    }

    GeneralContainer -> BaseVector;
    GeneralContainer -> BaseLinkedList;
    GeneralContainer -> BaseBinaryTree;
    GeneralContainer -> BaseHeap;
    GeneralContainer -> BaseHashTable;

    BaseHashTable -> BaseAVLTree [style=dashed, label="usa m_data"];
    BaseHeap -> BaseVector [style=dashed, label="usa m_data"];
    Drawable -> BaseLayout [style=dashed, label="proyecta"];
}
@enddot

---

## Estructuras Principales

| Categoría | Clases principales | Archivo |
| :--- | :--- | :--- |
| **Núcleo** | `IterableCore`, `ContainerCore`, `GeneralContainer`, `BaseView`, `GeneralNode`, `GeneralIterator` | `container_core.h`, `general_container.h`, `mixins.h` |
| **Secuenciales** | `BaseVector`, `VectorForwardIterator`, `VectorBackwardIterator` | `vector.h` |
| **Listas enlazadas** | `BaseLinkedList`, `BaseDoubleLinkedList`, `BaseCircularLinkedList`, `BaseCircularDoubleLinkedList` | `linked_list.h`, `double_linked_list.h`, `circular_linked_list.h`, `circular_double_linked_list.h` |
| **Árboles y montículos** | `BaseBinaryTree`, `BinaryTreeIterator`, `BaseAVLTree`, `BaseHeap`, `MinHeap`, `MaxHeap` | `binary_tree.h`, `avl_tree.h`, `heap.h` |
| **Criptografía y diccionarios** | `BaseMerkleTree`, `DefaultMerkleHasher`, `MerkleProofStep`, `BaseHashTable`, `HashEntry` | `merkle_tree.h`, `hash_table.h` |
| **Renderizado y layouts** | `GraphRenderer`, `ConsoleGraphRenderer`, `Canvas2D`, `VisualGraph`, `BaseLayout` | `render.h`, `layouts.h`, `ansi_style.h` |

---

## Navegación Rápida

- Explora la jerarquía de clases en la pestaña **Clases → Jerarquía de clases**.
- Consulta cada cabecera documentada en la pestaña **Archivos → Lista de archivos**.
- Haz clic sobre cualquier diagrama SVG dentro de las páginas de clases para ampliarlo a pantalla completa.
- Para la guía narrativa extendida y diagramas Mermaid del proyecto, consulta el repositorio en [GitHub](https://github.com/AldoLunaBueno/2026-II-CS-UNI-CC0E5-AEDAv).
