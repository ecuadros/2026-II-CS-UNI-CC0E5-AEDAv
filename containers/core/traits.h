#ifndef __CONTAINERS_TRAITS_H__
#define __CONTAINERS_TRAITS_H__

#include "../../types.h"
#include "../lists/listnodes.h"
#include "GeneralNode.h"
#include "../trees/binarytreeiterator.h"
#include "traversalops.h"
#include <concepts>
#include <functional>
#include <optional>
#include <string>
#include <type_traits>

/** Comparador válido que define un orden estricto débil sobre @p T. */
template <typename Comp, typename T>
concept ComparatorFor = std::strict_weak_order<Comp, T, T>;

/** Rasgos mínimos de un contenedor basado en nodos e iterador hacia adelante. */
template <typename Traits>
concept BasicContainerTraits = requires {
    typename Traits::value_type;
    typename Traits::Node;
    typename Traits::ForwardIterator;
};

/** Rasgos de un contenedor con recorrido bidireccional. */
template <typename Traits>
concept BidirectionalContainerTraits = BasicContainerTraits<Traits> && requires {
    typename Traits::BackwardIterator;
};

/** Rasgos de un contenedor ordenado mediante un comparador. */
template <typename Traits>
concept OrderedTraits = BasicContainerTraits<Traits> && requires {
    typename Traits::Compare;
    requires ComparatorFor<typename Traits::Compare, typename Traits::value_type>;
};

/** Rasgos válidos para BinaryTree y AVLTree. */
template <typename Traits>
concept BinaryTreeTraitsConcept = BidirectionalContainerTraits<Traits> && OrderedTraits<Traits> && requires {
    typename Traits::ref_type;
};

/** Rasgos válidos para Heap: almacenamiento contiguo con acceso indexado y tamaño. */
template <typename Traits>
concept HeapTraitsConcept = BidirectionalContainerTraits<Traits> && OrderedTraits<Traits> &&
    requires(typename Traits::Storage s, std::size_t i) {
        typename Traits::Storage;
        { s.size() } -> std::convertible_to<std::size_t>;
        { s.empty() } -> std::convertible_to<Bool>;
        s[i];
    };

/** Rasgos válidos para HashTable: clave, valor y árbol subyacente. */
template <typename Traits>
concept HashTableTraitsConcept = requires {
    typename Traits::key_type;
    typename Traits::mapped_type;
    typename Traits::Tree;
};

/** Rasgos válidos para BTree: orden >= 3, página, índice y comparador. */
template <typename Traits>
concept BTreeTraitsConcept = BidirectionalContainerTraits<Traits> && OrderedTraits<Traits> && requires {
    typename Traits::ref_type;
    typename Traits::Index;
    typename Traits::Page;
    requires Traits::order >= 3;
};

/** Contenedor cuyas búsquedas devuelven copias en `std::optional`, válidas aunque otro hilo lo modifique. */
template <typename C>
concept OptionalLookup = requires(const C& c, const typename C::value_type& key) {
    { c.find(key) } -> std::same_as<std::optional<typename C::value_type>>;
    { c.find_ref(key) } -> std::same_as<std::optional<typename C::ref_type>>;
};

/** Contenedor ordenado con `lower_bound`, `upper_bound` y `range(low, high, func)`, que devuelve la cantidad visitada. */
template <typename C>
concept RangeQueryable = requires(const C& c, const typename C::value_type& key) {
    { c.lower_bound(key) != c.end() } -> std::convertible_to<Bool>;
    { c.upper_bound(key) != c.end() } -> std::convertible_to<Bool>;
    *c.lower_bound(key);
    { c.range(key, key, [](const auto&) {}) } -> std::convertible_to<std::size_t>;
};

/** Tipo que se mueve e intercambia sin lanzar excepciones, por lo que los contenedores estándar lo mueven en vez de copiarlo. */
template <typename T>
concept NothrowMovable = std::is_nothrow_move_constructible_v<T> && std::is_nothrow_swappable_v<T>;

/** Rasgos válidos para BPlusTree: orden >= 3, página, índice y comparador. */
template <typename Traits>
concept BPlusTreeTraitsConcept = BidirectionalContainerTraits<Traits> && OrderedTraits<Traits> && requires {
    typename Traits::ref_type;
    typename Traits::Index;
    typename Traits::Page;
    requires Traits::order >= 3;
};

/** Rasgos válidos para Graph: vértice, peso, nodo y almacenamiento de vértices y aristas. */
template <typename Traits>
concept GraphTraitsConcept = requires {
    typename Traits::vertex_type;
    typename Traits::weight_type;
    typename Traits::Vertices;
    typename Traits::Edges;
    typename Traits::Index;
    requires std::is_arithmetic_v<typename Traits::weight_type>;
};

/** Rasgos válidos para RTree: tipo de coordenada, valor, rectángulo y máximo de entradas por nodo (al menos 4). */
template <typename Traits>
concept RTreeTraitsConcept = requires {
    typename Traits::value_type;
    typename Traits::ref_type;
    typename Traits::coord_type;
    typename Traits::Node;
    typename Traits::Index;
    requires std::is_arithmetic_v<typename Traits::coord_type>;
    requires Traits::order >= 4;
};

/** Rasgos válidos para MerkleTree: hasher con Digest, leaf() y combine(). */
template <typename Traits>
concept MerkleTraitsConcept = BidirectionalContainerTraits<Traits> &&
    requires(const typename Traits::value_type& v, const typename Traits::Digest& d) {
        typename Traits::Hasher;
        typename Traits::Digest;
        { Traits::Hasher::leaf(v) } -> std::convertible_to<typename Traits::Digest>;
        { Traits::Hasher::combine(d, d) } -> std::convertible_to<typename Traits::Digest>;
    };

/** Rasgos válidos para ExprTree: número, valor, nodo, iteradores y secuencia. */
template <typename Traits>
concept ExprTreeTraitsConcept = BidirectionalContainerTraits<Traits> && requires {
    typename Traits::number_type;
    typename Traits::template Sequence<typename Traits::value_type>;
};

/** Rasgos válidos para Sheet: número, tabla de celdas, fórmula y caché. */
template <typename Traits>
concept SheetTraitsConcept = requires {
    typename Traits::number_type;
    typename Traits::Cells;
    typename Traits::Formula;
    typename Traits::Cache;
};

template <typename T, Bool Reverse> class VectorIterator;
template <typename Traits, typename Ops = FullTraversalOps>
    requires BidirectionalContainerTraits<Traits>
class Vector;
template <typename T, typename Derived = void, typename R = Ref> class BinaryTreeNode;
template <typename T, typename R = Ref> class AVLNode;
template <typename Traits, typename Ops = SearchTraversalOps>
    requires BinaryTreeTraitsConcept<Traits>
class AVLTree;
template <typename Traits>
    requires HashTableTraitsConcept<Traits>
class HashTable;
template <typename Traits, typename Ops = ExprTraversalOps>
    requires ExprTreeTraitsConcept<Traits>
class ExprTree;
template <typename Number> struct ExprItem;

/**
 * Tipos que configuran un Vector: valor, nodo e iteradores.
 * @tparam T Tipo del valor.
 */
template <typename T> struct VectorTraits {
    using value_type = T;
    using Node = GeneralNode<T>;
    using ForwardIterator = VectorIterator<Node, false>;
    using BackwardIterator = VectorIterator<Node, true>;
};

/** Extrae `ref_type` de un nodo sin requerir que `AVLNode` o `BinaryTreeNode` estén completos. */
template <typename Node> struct node_ref_of { using type = typename Node::ref_type; };
template <typename T, typename Derived, typename R>
struct node_ref_of<BinaryTreeNode<T, Derived, R>> { using type = R; };
template <typename T, typename R>
struct node_ref_of<AVLNode<T, R>> { using type = R; };

/**
 * Tipos que configuran un árbol binario de búsqueda.
 * @tparam T Tipo de la clave.
 * @tparam Comparator Criterio de orden.
 * @tparam NodeType Tipo de nodo; cambiarlo permite reutilizar el árbol (por ejemplo, con AVLNode).
 */
template <typename T, typename Comparator = std::less<T>,
          typename NodeType = BinaryTreeNode<T, void>>
    requires ComparatorFor<Comparator, T>
struct BinaryTreeTraits {
    using value_type = T;
    using Node = NodeType;
    using ref_type = typename node_ref_of<NodeType>::type;
    using Compare = Comparator;
    using ForwardIterator = BinaryTreeIterator<const Node>;
    using BackwardIterator = BinaryTreeIterator<const Node, TreeOrder::InOrder, true>;
};

/**
 * Traits de un AVL: los de BinaryTreeTraits con AVLNode como nodo.
 * @tparam T Tipo de la clave.
 * @tparam Comparator Criterio de orden.
 * @tparam R Tipo de la referencia guardada junto a cada clave.
 */
template <typename T, typename Comparator = std::less<T>, typename R = Ref>
using AVLTraits = BinaryTreeTraits<T, Comparator, AVLNode<T, R>>;

/**
 * Tipos comunes a las listas enlazadas.
 * @tparam T Tipo del valor.
 * @tparam NodeType Tipo de nodo.
 * @tparam Comparator Criterio de orden.
 */
template <typename T, typename NodeType, typename Comparator> struct ListTraits {
    using value_type = T;
    using Node = NodeType;
    using ForwardIterator = LinkedListForwardIterator<T, Node>;
    using Compare = Comparator;
};

/**
 * Tipos que configuran una HashTable.
 * @tparam Key Tipo de la clave.
 * @tparam Value Tipo del valor asociado.
 * @tparam Comparator Criterio de orden de las claves.
 * @tparam TreeType Árbol que guarda las entradas; por defecto un AVL, pero sirve cualquier árbol con la misma interfaz.
 */
template <typename Key, typename Value, typename Comparator = std::less<Key>,
          typename TreeType = AVLTree<AVLTraits<Key, Comparator, Value>>>
    requires ComparatorFor<Comparator, Key>
struct HashTableTraits {
    using key_type = Key;
    using mapped_type = Value;
    using Tree = TreeType;
};

/** Traits de la lista simplemente enlazada. */
template <typename T, typename Comparator = std::less<T>>
    requires ComparatorFor<Comparator, T>
struct LinkedListTraits : ListTraits<T, LinkedListNode<T>, Comparator> {};

/** Traits de la lista doblemente enlazada: añade el iterador hacia atrás. */
template <typename T, typename Comparator = std::less<T>>
    requires ComparatorFor<Comparator, T>
struct DoubleLinkedListTraits : ListTraits<T, DoubleLinkedNode<T>, Comparator> {
    using BackwardIterator = DoubleLinkedListBackwardIterator<T>;
};

/**
 * Tipos que configuran un Heap, que guarda sus elementos en un Vector.
 * @tparam T Tipo del valor.
 * @tparam Comparator Criterio que decide qué elemento queda en la raíz.
 */
template <typename T, typename Comparator = std::less<T>>
    requires ComparatorFor<Comparator, T>
struct HeapTraits {
    using value_type = T;
    using Storage = Vector<VectorTraits<T>>;
    using Node = typename Storage::Node;
    using Compare = Comparator;
    using ForwardIterator = typename Storage::ConstForwardIterator;
    using BackwardIterator = typename Storage::ConstBackwardIterator;
};

template <typename Traits> class BTreePage;
template <typename Page, TreeOrder Order, Bool Reverse> class BTreeIterator;

/**
 * Tipos que configuran un árbol B.
 * @tparam T Tipo de la clave.
 * @tparam Comparator Criterio de orden.
 * @tparam Order Máximo de claves por página (la raíz admite `2 * Order + 1`); al menos 3.
 * @tparam R Tipo de la referencia guardada junto a cada clave.
 */
template <typename T, typename Comparator = std::less<T>, std::size_t Order = 3, typename R = Ref>
    requires ComparatorFor<Comparator, T> && (Order >= 3)
struct BTreeTraits {
    using value_type = T;
    using ref_type = R;
    using Compare = Comparator;
    using Node = GeneralNode<T, R>;
    /** Posiciones y cantidades dentro de una página; con signo porque se recorren vecinas a izquierda y derecha. */
    using Index = std::ptrdiff_t;
    using Page = BTreePage<BTreeTraits>;
    /** Iterador sobre @p P (constante o no) con el orden de recorrido indicado. */
    template <typename P, TreeOrder O = TreeOrder::InOrder, Bool Reverse = false>
    using Iterator = BTreeIterator<P, O, Reverse>;
    using ForwardIterator = Iterator<Page>;
    using BackwardIterator = Iterator<Page, TreeOrder::InOrder, true>;
    using ConstForwardIterator = Iterator<const Page>;
    using ConstBackwardIterator = Iterator<const Page, TreeOrder::InOrder, true>;
    static constexpr Index order = Order;
};

template <typename Traits> class BPlusNode;
template <typename Node, Bool Reverse> class BPlusIterator;

/**
 * Tipos que configuran un árbol B+: todas las claves viven en las hojas, que están enlazadas entre sí.
 * @tparam T Tipo de la clave.
 * @tparam Comparator Criterio de orden.
 * @tparam Order Máximo de claves por nodo; al menos 3.
 * @tparam R Tipo de la referencia guardada junto a cada clave.
 */
template <typename T, typename Comparator = std::less<T>, std::size_t Order = 4, typename R = Ref>
    requires ComparatorFor<Comparator, T> && (Order >= 3)
struct BPlusTreeTraits {
    using value_type = T;
    using ref_type = R;
    using Compare = Comparator;
    using Node = GeneralNode<T, R>;
    using Index = std::ptrdiff_t;
    using Page = BPlusNode<BPlusTreeTraits>;
    using ForwardIterator = BPlusIterator<Page, false>;
    using BackwardIterator = BPlusIterator<Page, true>;
    using ConstForwardIterator = BPlusIterator<const Page, false>;
    using ConstBackwardIterator = BPlusIterator<const Page, true>;
    static constexpr Index order = Order;
};

template <typename Digest> class MerkleNode;
template <typename T> struct StdMerkleHasher;

/**
 * Tipos que configuran un MerkleTree.
 * @tparam T Tipo de los bloques de datos.
 * @tparam HasherType Política de hash, con `Digest`, `leaf()` y `combine()`.
 */
template <typename T, typename HasherType = StdMerkleHasher<T>>
struct MerkleTreeTraits {
    using value_type = T;
    using Hasher = HasherType;
    using Digest = typename Hasher::Digest;
    using Node = MerkleNode<Digest>;
    using ForwardIterator = BinaryTreeIterator<const Node, TreeOrder::PostOrder>;
    using BackwardIterator = BinaryTreeIterator<const Node, TreeOrder::PostOrder, true>;
};

/**
 * Tipos que configuran un ExprTree, el árbol de sintaxis de una fórmula.
 * @tparam Number Tipo numérico de los resultados.
 * @tparam SequenceType Traits de la secuencia que usa para tokens y para la pila de evaluación.
 */
template <typename Number = Real, template <typename> typename SequenceType = VectorTraits>
struct ExprTreeTraits {
    using number_type = Number;
    using value_type = ExprItem<Number>;
    using Node = BinaryTreeNode<value_type>;
    using ForwardIterator = BinaryTreeIterator<const Node, TreeOrder::PostOrder>;
    using BackwardIterator = BinaryTreeIterator<const Node, TreeOrder::PostOrder, true>;
    template <typename U> using Sequence = Vector<SequenceType<U>>;
};

/**
 * Tipos que configuran una Sheet: las piezas con las que se arma la hoja.
 * @tparam Number Tipo numérico de los resultados.
 * @tparam CellsType Tabla nombre de celda -> texto escrito.
 * @tparam SequenceType Traits de la secuencia usada por `FormulaType` para tokens y argumentos.
 * @tparam FormulaType Árbol que lee y evalúa cada fórmula.
 * @tparam CacheType Mapa temporal usado por consulta para memoizar valores evaluados.
 */
template <typename Number = Real,
          typename CellsType = HashTable<HashTableTraits<std::string, std::string>>,
          template <typename> typename SequenceType = VectorTraits,
          typename FormulaType = ExprTree<ExprTreeTraits<Number, SequenceType>>,
          typename CacheType = HashTable<HashTableTraits<std::string, typename FormulaType::Value>>>
struct SheetTraits {
    using number_type = Number;
    using Cells = CellsType;
    using Formula = FormulaType;
    using Cache = CacheType;
    template <typename U> using Sequence = Vector<SequenceType<U>>;
};

template <typename Coord> struct Rect;
template <typename Traits> class RTreeNode;

/**
 * Tipos que configuran un Graph: vértices con posición, aristas con peso y un índice clave -> vértice.
 * @tparam V Tipo de la clave que identifica a un vértice.
 * @tparam W Tipo numérico del peso de las aristas.
 * @tparam Comparator Criterio de orden de las claves, que usa el índice.
 */
template <typename V, typename W = Real, typename Comparator = std::less<V>>
    requires ComparatorFor<Comparator, V> && std::is_arithmetic_v<W>
struct GraphTraits {
    using vertex_type = V;
    using weight_type = W;
    using Compare = Comparator;
    using Index = std::ptrdiff_t;
    /** Vértice: su clave, su posición en el espacio y la primera y última arista que salen de él. */
    struct Vertex {
        V key{};
        Real x = 0, y = 0, z = 0;
        Index head = -1, tail = -1;
    };
    /** Arista dirigida; las que salen de un mismo vértice forman una lista enlazada por `next`. */
    struct Edge {
        Index to = 0;
        W weight{};
        Index next = -1;
        Bool alive = true;
    };
    using Vertices = Vector<VectorTraits<Vertex>>;
    using Edges = Vector<VectorTraits<Edge>>;
    using Lookup = HashTable<HashTableTraits<V, Index, Comparator>>;
    template <typename U> using Sequence = Vector<VectorTraits<U>>;
};

/**
 * Tipos que configuran un RTree de rectángulos en el plano.
 * @tparam Coordinate Tipo numérico de las coordenadas.
 * @tparam Order Máximo de entradas por nodo; al menos 4.
 * @tparam R Tipo de la referencia guardada junto a cada rectángulo.
 */
template <typename Coordinate = Real, std::size_t Order = 4, typename R = Ref>
    requires std::is_arithmetic_v<Coordinate> && (Order >= 4)
struct RTreeTraits {
    using coord_type = Coordinate;
    using value_type = Rect<Coordinate>;
    using ref_type = R;
    using Node = GeneralNode<value_type, R>;
    using Index = std::ptrdiff_t;
    using Page = RTreeNode<RTreeTraits>;
    template <typename U> using Sequence = Vector<VectorTraits<U>>;
    static constexpr Index order = Order;
};

#endif // __CONTAINERS_TRAITS_H__
