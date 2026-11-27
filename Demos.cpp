
#include "types.h"
#include <iostream>
#include <set>
#include <map>
#include <cstring>
#include <random>
#include <numeric>
#include <limits>
#include <atomic>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <string>
#include <cctype>
#include <thread>
#include <vector>
#include <algorithm>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <string_view>
#include <iomanip>
#include "containers/aliases.h"
#include "containers/hashing/hashtable.h"
#include "containers/hashing/sha256.h"
#include "Demos.h"

/** Lista de pares (valor, ref) para llenar un contenedor. */
template <typename Container>
using Values = std::vector<std::pair<typename Container::value_type, Ref>>;

/** Contenedor con `insert(valor, ref)`. */
template <typename C>
concept Insertable = requires(C& c, const typename C::value_type& v, Ref r) { c.insert(v, r); };

/** Contenedor con `top()` y `pop()`. */
template <typename C>
concept HeapLike = requires(C& c) {
    c.top();
    c.pop();
};

/** Árbol de búsqueda: recorridos, altura, `remove` y dibujo. */
template <typename C>
concept TreeLike = requires(C& t, TX key, std::ostream& os) {
    t.InOrder();
    t.PreOrder();
    t.PostOrder();
    t.height();
    t.remove(key);
    t.print_tree(os, visual::Layout::Sideways);
};

/** Árbol que ofrece los seis recorridos (tres órdenes y sus inversos). */
template <typename C>
concept SixOrderTree = requires(const C& t) {
    t.InOrder();
    t.ReverseInOrder();
    t.PreOrder();
    t.ReversePreOrder();
    t.PostOrder();
    t.ReversePostOrder();
};

/** Contenedor con interfaz de mapa al estilo de `std::map`. */
template <typename M>
concept MapLike = requires(M& m, const M& cm, const typename M::key_type& key, const typename M::value_type& entry,
                           typename M::iterator it) {
    { m[key] } -> std::same_as<typename M::mapped_type&>;
    { m.at(key) } -> std::same_as<typename M::mapped_type&>;
    { cm.at(key) } -> std::same_as<const typename M::mapped_type&>;
    { m.insert(entry).second } -> std::convertible_to<Bool>;
    { m.emplace(entry).second } -> std::convertible_to<Bool>;
    { m.find(key) } -> std::same_as<typename M::iterator>;
    { cm.find(key) } -> std::same_as<typename M::const_iterator>;
    { m.erase(key) } -> std::convertible_to<std::size_t>;
    { m.erase(it) } -> std::same_as<typename M::iterator>;
    { cm.contains(key) } -> std::convertible_to<Bool>;
    { cm.size() } -> std::convertible_to<std::size_t>;
    { it == m.end() } -> std::convertible_to<Bool>;
    { ++it } -> std::same_as<typename M::iterator&>;
    { it++ } -> std::same_as<typename M::iterator>;
    { it->first } -> std::same_as<const typename M::key_type&>;
    { it->second } -> std::same_as<typename M::mapped_type&>;
    { (*it).second } -> std::same_as<typename M::mapped_type&>;
    { cm.begin()->second } -> std::same_as<const typename M::mapped_type&>;
    { cm.rbegin() == cm.rend() } -> std::convertible_to<Bool>;
    requires std::convertible_to<typename M::iterator, typename M::const_iterator>;
};

/** Árbol de Merkle: raíz, pruebas de inclusión, verificación, `update`, `push_back` y lectura/escritura. */
template <typename M>
concept MerkleLike = requires(M& m, const M& cm, std::size_t index, const typename M::value_type& block,
                              const typename M::value_type* first, const typename M::Digest& root, std::ostream& os,
                              std::istream& is) {
    os << cm;
    is >> m;
    requires std::equality_comparable<typename M::Digest>;
    requires std::copy_constructible<M>;
    M(first, first);
    { cm.root_hash() } -> std::same_as<typename M::Digest>;
    { cm.proof(index) } -> std::same_as<typename M::Proof>;
    { M::verify(block, cm.proof(index), root) } -> std::same_as<Bool>;
    m.update(index, block);
    m.push_back(block);
    m.clear();
    { cm.size() } -> std::convertible_to<std::size_t>;
    { cm.height() } -> std::convertible_to<std::size_t>;
    { cm.empty() } -> std::convertible_to<Bool>;
};

/** Contenedor que ofrece `ApplyFunction`. */
template <typename C>
concept Mutable = requires(C& c) { c.ApplyFunction([](auto&) {}); };

/** Lista circular. */
template <typename C>
concept Circular = visual::CircularView<C>;

/** Contenedor que se puede copiar y asignar. */
template <typename C>
concept Copyable = std::is_copy_constructible_v<C> && std::is_copy_assignable_v<C>;

/** Devuelve la clave de un valor: `value.key()` si existe, o el propio valor. */
template <typename T> auto KeyOf(const T& value) {
    if constexpr (requires { value.key(); })
        return value.key();
    else
        return value;
}

/** Suma 1 al valor del nodo. */
void AddOne(GeneralNode<TX>& node) {
    node.value() += 1;
}

/** Suma @p x al valor del nodo. */
template <typename T> void AddX(GeneralNode<T>& node, T x) {
    node.value() += x;
}

/** Eleva al cuadrado el valor del nodo. */
void Square(GeneralNode<TX>& node) {
    node.value() *= node.value();
}

/** Escribe el nodo seguido de un espacio. */
template <typename Node> void PrintNode(const Node& node, std::ostream& os) {
    os << node << " ";
}
/** Cantidad de hilos de las pruebas de concurrencia. */
const Int NThreads = 5;

/** Imprime @p value en una línea con el color ANSI @p color. */
template <typename Value>
void PrintColor(const Value& value, std::string_view color = "1;36", std::ostream& os = std::cout) {
    os << visual::Painter(os)(color, visual::to_text(value)) << '\n';
}

/** Ejecuta @p function y borra el archivo @p filename al terminar, también si lanza. */
template <typename Function> void WithDemoFile(const std::string& filename, Function function) {
    auto removeFile = [&filename] {
        if (std::remove(filename.c_str()) != 0 && errno != ENOENT)
            std::cerr << "No se pudo borrar " << filename << '\n';
    };
    try {
        function();
    } catch (...) {
        removeFile();
        throw;
    }
    removeFile();
}

/**
 * Escribe el contenedor en un archivo.
 * @throws std::runtime_error si no se puede crear o escribir.
 */
template <typename Container> void SaveTo(const std::string& filename, const Container& container) {
    std::ofstream file(filename, std::ios::trunc);
    if (!file) throw std::runtime_error("No se pudo crear " + filename);
    file << container << '\n';
    if (!file) throw std::runtime_error("No se pudo escribir " + filename);
}

/**
 * Lee el contenedor desde un archivo.
 * @throws std::runtime_error si no se puede abrir o leer.
 */
template <typename Container> void LoadFrom(const std::string& filename, Container& container) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("No se pudo abrir " + filename);
    if (!(file >> container)) throw std::runtime_error("No se pudo leer " + filename);
}

/** Inserta con `insert` todos los pares de @p values. */
template <typename Container> void InsertAll(Container& container, const Values<Container>& values) {
    for (const auto& [value, ref] : values)
        container.insert(value, ref);
}

/** Par de iteradores utilizable en un `for` por rango. */
template <typename Iterator> struct Range {
    Iterator first, last;
    Iterator begin() const { return first; }
    Iterator end() const { return last; }
};

/** Devuelve el rango inverso (`rbegin()`, `rend()`) del contenedor. */
template <typename Container> auto ReverseRange(const Container& container) {
    return Range{container.rbegin(), container.rend()};
}

/** Imprime `etiqueta: [ ... ]`, donde el contenido lo escribe @p body. */
template <typename Body> void PrintBracketed(const std::string& label, Body body) {
    std::cout << label << ": [";
    body();
    std::cout << "]\n";
}

/** Imprime todos los nodos de un recorrido entre corchetes. */
template <typename Traversal>
void PrintTraversal(const std::string& label, const Traversal& traversal) {
    PrintBracketed(label, [&] {
        for (auto&& node : traversal)
            std::cout << node << " ";
    });
}

/** Devuelve las referencias de los nodos en el orden del recorrido. */
template <typename Traversal> std::vector<Ref> CollectRefs(const Traversal& traversal) {
    std::vector<Ref> refs;
    for (auto&& node : traversal)
        refs.push_back(node.getRef());
    return refs;
}

/**
 * Comprueba que @p backward sea exactamente @p forward al revés.
 * @throws std::runtime_error si no coinciden.
 */
template <typename Forward, typename Backward>
void CheckReverse(const std::string& name, const Forward& forward, const Backward& backward) {
    std::vector<Ref> refs = CollectRefs(backward);
    std::reverse(refs.begin(), refs.end());
    if (refs != CollectRefs(forward))
        throw std::runtime_error(name + ": el recorrido inverso no coincide");
}

/** Imprime una vuelta de una lista circular como `a -> b -> (vuelve a a)`. */
template <typename Iterator> void PrintCircularTraversal(Iterator begin, Iterator end) {
    if (begin == end) {
        std::cout << "[]\n";
        return;
    }

    std::cout << *begin;
    auto it = begin;
    for (++it; it != end; ++it)
        std::cout << " -> " << *it;
    std::cout << " -> (vuelve a " << *begin << ")\n";
}

/** Llena el contenedor, lo guarda en un archivo y lo imprime. */
template <typename Container>
void TestContainer(Container& container, const Values<Container>& values,
                   const std::string& filename) {
    WithDemoFile(filename, [&] {
        PrintColor("TestContainer (" + filename + ")");
        fill_from(container, values);

        SaveTo(filename, container);

        std::cout << "Container using cout directly: ";
        PrintColor(container);
    });
}

/** Prueba `call`, `rcall`, los iteradores y `FirstThat`. */
template <typename Container>
void TestTraversal(Container& container, const std::string& name) {
    PrintColor("TestTraversal (" + name + ")");
    using Node = typename Container::Node;

    PrintBracketed("Forward traversal ", [&] { container.call(PrintNode<Node>, std::cout); });
    PrintTraversal("Forward iterators ", container);

    if constexpr (requires { container.rcall(PrintNode<Node>, std::cout); }) {
        PrintBracketed("Backward traversal", [&] { container.rcall(PrintNode<Node>, std::cout); });
        PrintTraversal("Backward iterators", ReverseRange(container));
    }

    auto it = container.begin();
    if (it != container.end() && ++it != container.end()) {
        Ref target = (*it).getRef();
        const Node& found = container.FirstThat(
            [](const Node& node, Ref ref) { return node.getRef() == ref; }, target);
        std::cout << "FirstThat (ref " << target << "): " << found << '\n';
    }
}

/** Dibuja el contenedor; los heaps se muestran además como arreglo. */
template <typename Container>
void TestVisualize(Container& container, const std::string& name) {
    PrintColor("TestVisualize (" + name + ")");
    container.visualize(std::cout);
    if constexpr (visual::HasTreeView<Container> && !TreeLike<Container>) {
        std::cout << "Como arreglo:\n";
        container.print_linear(std::cout);
    }
}

/** Prueba `push_back`, `insert`, copia, asignación, guardado, lectura y reutilización tras `clear`. */
template <typename Container>
void TestGenericContainer(Container& container, const std::string& filename = "container.txt") {
    WithDemoFile(filename, [&] {
        PrintColor("TestGenericContainer (" + filename + ")");
        std::cout << "Vacio: " << container.empty() << '\n';

        fill_from(container, Values<Container>{{1, 101}, {3, 103}, {7, 107}});
        std::cout << "Luego de push_back: ";
        PrintColor(container);

        if constexpr (Insertable<Container>) {
            InsertAll(container, {{5, 105}, {2, 102}, {9, 109}});
            std::cout << "Luego de insert: ";
            PrintColor(container);

            Container copia(container);
            Container asignada;
            asignada = container;
            copia.insert(4, 104);
            std::cout << "Original: ";
            PrintColor(container);
            std::cout << "Copia modificada: ";
            PrintColor(copia);
            std::cout << "Asignada: ";
            PrintColor(asignada);
        }

        SaveTo(filename, container);
        std::cout << "Escrito en " << filename << '\n';

        Container leida;
        LoadFrom(filename, leida);
        std::cout << "Leido desde " << filename << ": " << leida << '\n';

        Container reutilizada;
        reutilizada.push_back(1, 101);
        reutilizada.clear();
        std::cout << "Luego de clear, vacia: " << reutilizada.empty() << '\n';
        reutilizada.push_back(10, 110);
        std::cout << "Luego de reutilizarla: ";
        PrintColor(reutilizada);
    });
}

/** Comprueba que una lectura con formato inválido falla y no modifica el contenedor. */
template <typename Container>
void TestInvalidRead(Container& container, const std::string& name) {
    std::ostringstream before;
    before << container;
    std::istringstream invalid("[(7,70),broken]");
    if (invalid >> container) throw std::runtime_error(name + ": lectura invalida aceptada");
    std::ostringstream after;
    after << container;
    if (before.str() != after.str())
        throw std::runtime_error(name + ": la lectura invalida cambio los datos");
}

/** Comprueba `clear` repetido, la copia, la autoasignación y la reutilización del contenedor. */
template <typename Container>
void TestCopyAndClear(Container& container, const std::string& name) {
    PrintColor("TestCopyAndClear (" + name + ")");
    container.clear();
    container.push_back(8, 80);
    container.clear();
    container.clear();
    if (!container.empty() || container.begin() != container.end())
        throw std::runtime_error(name + ": clear no dejo el contenedor vacio");

    container.push_back(3, 30);
    if constexpr (Insertable<Container>)
        InsertAll(container, {{1, 10}, {5, 50}});
    Container copy(container);
    container = container;
    if (copy.empty() || container.empty()) throw std::runtime_error(name + ": fallo la copia");

    container.clear();
    container.push_back(9, 90);
    if (container.empty() || KeyOf((*container.begin()).getValue()) != 9)
        throw std::runtime_error(name + ": no se pudo reutilizar el contenedor");
    std::cout << name << ": clear y copia OK\n";
}

/** Inserta @p values desde varios hilos y comprueba que no se pierde ningún elemento. */
template <typename Container>
void TestRaceCondition(const std::string& name, const Values<Container>& values) {
    PrintColor("TestRaceCondition (" + name + ")");
    Container container;

    size_t n = values.size();
    std::vector<std::thread> workers;
    for (Int t = 0; t < NThreads; ++t) {
        workers.emplace_back([&container, &values, n, t]() {
            for (size_t i = t; i < n; i += NThreads)
                container.push_back(values[i].first, values[i].second);
        });
    }
    for (auto& worker : workers)
        worker.join();

    Ref expectedSum = 0;
    for (auto& v : values)
        expectedSum += KeyOf(v.first);

    size_t actualCount = 0;
    Ref actualSum = 0;
    for (auto&& node : container) {
        ++actualCount;
        actualSum += KeyOf(node.getValue());
    }

    std::cout << "DemoRaceCondition (" << name << "): se esperaban " << n
              << " elementos, el Container quedo con " << actualCount << '\n';
    std::cout << "  suma esperada = " << expectedSum << ", suma obtenida = " << actualSum << '\n';

    if (actualCount != n || actualSum != expectedSum)
        std::cout << "  *** Race condition detectada: se perdieron inserciones (push_back sin "
                     "sincronizar) ***\n";
    else
        std::cout << "  No se perdio ningun elemento: el mutex de push_back "
                  << "sincroniza correctamente las inserciones concurrentes\n";
}

/** Ejecuta todas las pruebas genéricas sobre el contenedor. */
template <typename Container>
void TestStructure(Container& container, const std::string& name) {
    PrintColor(name);
    const Values<Container> values{{5, 50}, {1, 10}, {4, 40}, {2, 20}, {7, 70}, {5, 51}};

    TestContainer(container, values, name + ".txt");
    TestTraversal(container, name);
    TestVisualize(container, name);

    if constexpr (HeapLike<Container>) {
        std::cout << "Top: " << container.top() << '\n';
        std::cout << "Orden de extraccion: ";
        while (!container.empty())
            std::cout << container.pop() << ' ';
        std::cout << '\n';
    }

    container.clear();
    TestGenericContainer(container, name + "_persist.txt");
    TestInvalidRead(container, name);

    if constexpr (Copyable<Container>) {
        Container copyAndClear;
        TestCopyAndClear(copyAndClear, name);
    }

    TestRaceCondition<Container>(name, values);
}

/** Ejecuta las pruebas genéricas sobre un contenedor recién creado. */
template <typename Container> void TestStructure(const std::string& name) {
    Container container;
    TestStructure(container, name);
}

/** Prueba de concurrencia con 200000 inserciones repartidas entre los hilos. */
template <typename Container> void DemoRaceCondition(const std::string& name) {
    PrintColor("DemoRaceCondition (" + name + ")");
    const size_t N = 200000;
    Values<Container> values(N);
    for (size_t i = 0; i < N; ++i)
        values[i] = {static_cast<TX>(i), static_cast<Ref>(i)};

    std::cout << "DemoRaceCondition (" << NThreads << " hilos, push_back):\n";
    TestRaceCondition<Container>(name, values);
}


/** Imprime el ciclo de una lista circular, en ambos sentidos si es doble. */
template <Circular List> void PrintCycles(const List& list, const std::string& label) {
    if constexpr (ReverseTraversable<List>) {
        std::cout << "Ciclo" << label << " hacia adelante: ";
        PrintCircularTraversal(list.begin(), list.end());
        std::cout << "Ciclo" << label << " hacia atras: ";
        PrintCircularTraversal(list.rbegin(), list.rend());
    } else {
        std::cout << "Ciclo" << label << ": ";
        PrintCircularTraversal(list.begin(), list.end());
    }
}

/** Prueba `ApplyFunction` con AddOne y AddX. */
template <Mutable Container> void TestApplyFunction(Container& container) {
    container.ApplyFunction(AddOne);
    std::cout << "Tras AddOne: ";
    PrintColor(container);
    container.ApplyFunction(AddX<TX>, TX(10));
    std::cout << "Tras AddX(10): ";
    PrintColor(container);
}

/** Prueba el contenedor con el comparador descendente. */
template <Insertable Desc> void TestDescending(const std::string& label) {
    Desc descendente;
    InsertAll(descendente, {{5, 105}, {3, 103}, {7, 107}});
    std::cout << label << ": ";
    PrintColor(descendente);
    if constexpr (Circular<Desc>)
        PrintCycles(descendente, " descendente");
    if constexpr (TreeLike<Desc>)
        descendente.print_tree(std::cout);
}

/**
 * Comprueba que el inorden esté ordenado y que el tamaño coincida.
 * @throws std::runtime_error si el árbol perdió su orden.
 */
template <TreeLike Tree> void CheckTree(const Tree& tree, const std::string& name) {
    std::vector<TX> values;
    for (auto&& node : tree.InOrder())
        values.push_back(KeyOf(node.getValue()));
    if (!std::is_sorted(values.begin(), values.end()) || values.size() != tree.size())
        throw std::runtime_error(name + ": el arbol perdio su orden");
}

/** Imprime los seis recorridos y comprueba que cada inverso coincide con su directo. */
template <SixOrderTree Tree> void TestTreeTraversals(const Tree& tree, const std::string& name) {
    PrintColor("TestTreeTraversals (" + name + ")");
    using Node = typename Tree::Node;
    PrintTraversal("InOrder         ", tree.InOrder());
    PrintTraversal("ReverseInOrder  ", tree.ReverseInOrder());
    PrintTraversal("PreOrder        ", tree.PreOrder());
    PrintTraversal("ReversePreOrder ", tree.ReversePreOrder());
    PrintTraversal("PostOrder       ", tree.PostOrder());
    PrintTraversal("ReversePostOrder", tree.ReversePostOrder());

    PrintBracketed("for_each PreOrder", [&] {
        auto preOrder = tree.PreOrder();
        traversal_detail::for_each(preOrder.begin(), preOrder.end(), PrintNode<Node>, std::cout);
    });

    CheckReverse(name + " InOrder", tree.InOrder(), tree.ReverseInOrder());
    CheckReverse(name + " PreOrder", tree.PreOrder(), tree.ReversePreOrder());
    CheckReverse(name + " PostOrder", tree.PostOrder(), tree.ReversePostOrder());
    std::cout << name << ": los 6 recorridos OK\n";
}

/** Prueba `remove` con una hoja, un nodo con un hijo, la raíz y un valor inexistente, y la persistencia posterior. */
template <TreeLike Tree> void TestTreeRemove(Tree& tree, const std::string& name) {
    PrintColor("TestTreeRemove (" + name + ")");
    for (TX value : {20, 60, 50, 99}) {
        Bool removed = tree.remove(value);
        std::cout << "remove(" << value << "): " << removed << '\n';
        if (tree.contains(value))
            throw std::runtime_error(name + ": remove no borro el valor");
        CheckTree(tree, name);
    }
    tree.print_tree(std::cout, visual::Layout::Sideways);

    std::stringstream stream;
    stream << tree;
    Tree leido;
    stream >> leido;
    std::cout << "Persistencia luego de remove: " << leido << '\n';
    CheckTree(leido, name);
}

/** Inserta 1..15 en orden y muestra la altura y el dibujo del árbol. */
template <TreeLike Tree> void TestTreeHeight(const std::string& name) {
    PrintColor("TestTreeHeight (" + name + ")");
    Tree tree;
    for (TX value = 1; value <= 15; ++value)
        tree.insert(value, value * 10);
    std::cout << "Insertando 1..15 en orden, altura = " << tree.height() << '\n';
    tree.print_tree(std::cout, visual::Layout::Sideways);
    tree.print_tree(std::cout);
    tree.print_tree(std::cout, visual::Layout::Cursor);
    CheckTree(tree, name);
}

/** Ejecuta las pruebas específicas de árboles: dibujo, recorridos, eliminación y altura. */
template <TreeLike Tree> void TestTree(const std::string& name) {
    Tree tree{{50, 500}, {30, 300}, {70, 700}, {20, 200}, {40, 400},
              {60, 600}, {80, 800}, {35, 350}, {65, 650}};
    PrintColor("Visualizacion (" + name + ")");
    tree.print_tree(std::cout, visual::Layout::Sideways);
    tree.print_tree(std::cout);
    tree.print_tree(std::cout, visual::Layout::Cursor);
    TestTreeTraversals(tree, name);
    TestTreeRemove(tree, name);
    TestTreeHeight<Tree>(name);
}

/**
 * Ejecuta sobre @p container todas las pruebas que su tipo admite.
 * @tparam Desc Variante con orden descendente del contenedor, o `void` para omitir esa prueba.
 * @tparam C Tipo del contenedor.
 */
template <typename Desc = void, typename C> void Demo(C& container, const std::string& name) {
    PrintColor("Demo" + name);
    TestStructure(container, name);

    if constexpr (Circular<C>)
        PrintCycles(container, "");

    if constexpr (Mutable<C> && Insertable<C>)
        TestApplyFunction(container);

    if constexpr (!std::is_void_v<Desc>)
        TestDescending<Desc>(name + " descendente");

    if constexpr (TreeLike<C>)
        TestTree<C>(name);

    if constexpr (Mutable<C> && Insertable<C>)
        DemoRaceCondition<C>(name);
}

/** Crea un contenedor de tipo @p C y ejecuta sus pruebas. */
template <typename C, typename Desc = void> void Demo(const std::string& name) {
    C container;
    Demo<Desc>(container, name);
}

/** Texto de la clave de un valor. */
template <typename T> std::string KeyText(const T& value) {
    if constexpr (requires { value.key(); })
        return visual::to_text(value.key());
    else
        return visual::to_text(value);
}

/** Tipo de la clave que guardan los nodos de un árbol. */
template <typename Tree>
using TreeKey = std::remove_cvref_t<decltype(std::declval<const Tree&>().PreOrder().begin()->getValue())>;

/**
 * Copia sin balancear de un árbol, para mostrar cómo quedaría antes de las rotaciones del AVL.
 * @tparam Tree Tipo del árbol original.
 */
template <typename Tree>
class Unbalanced : public BinaryTree<BinaryTreeTraits<TreeKey<Tree>, typename Tree::Compare,
                                                       BinaryTreeNode<TreeKey<Tree>, void, typename Tree::ref_type>>> {
    using Parent = BinaryTree<BinaryTreeTraits<TreeKey<Tree>, typename Tree::Compare,
                                               BinaryTreeNode<TreeKey<Tree>, void, typename Tree::ref_type>>>;
    using typename Parent::Node;
    using typename Parent::NodePtr;

    mutable const Node* m_mark = nullptr;

    /** Factor de balance de @p n calculado a partir de las alturas. */
    Int balance(const Node* n) const {
        return static_cast<Int>(this->height(n->m_pChild[1])) - static_cast<Int>(this->height(n->m_pChild[0]));
    }

    /** Elige el reemplazo igual que el AVL, para que ambos árboles eliminen el mismo nodo. */
    NodePtr replacement(NodePtr node) const override {
        Bool side = balance(node) >= 0;
        return extreme(node->m_pChild[side], !side);
    }

    /** Escribe la clave con su factor de balance; el nodo marcado va entre `<` y `>`. */
    void print_node(std::ostream& os, const Node& node) const override {
        Bool marked = &node == m_mark;
        Int bf = balance(&node);
        os << (marked ? "<" : "") << KeyText(node.getValue()) << '[' << (bf > 0 ? "+" : "") << bf << ']'
           << (marked ? ">" : "");
    }

public:
    /** Copia @p tree insertando sus nodos en preorden, lo que conserva su forma. */
    explicit Unbalanced(const Tree& tree) {
        for (auto&& node : tree.PreOrder())
            this->insert(node.getValue(), node.getRef());
    }

    /** Devuelve el primer nodo desbalanceado en postorden, o `nullptr` si no hay ninguno. */
    const Node* unbalanced() const {
        for (auto&& node : this->PostOrder())
            if (std::abs(balance(&node)) > 1) return &node;
        return nullptr;
    }

    /** Explica el caso de desbalance de @p x (LL, RR, LR o RL) y la rotación que lo corrige. */
    std::string describe(const Node* x) const {
        Int bf = balance(x);
        Bool right = bf > 0;
        const Node* c = x->m_pChild[right];
        Bool twice = balance(c) * bf < 0;
        std::string nx = KeyText(x->getValue()), nc = KeyText(c->getValue());
        std::string side = right ? "derecha" : "izquierda";
        std::string other = right ? "izquierda" : "derecha";
        std::ostringstream ss;
        ss << "Nodo " << nx << " desbalanceado (factor " << (bf > 0 ? "+" : "") << bf << "), cargado a la "
           << side << '\n';
        if (twice)
            ss << "Caso " << (right ? "RL" : "LR") << ": rotacion doble -> primero a la " << side << " en " << nc
               << ", luego a la " << other << " en " << nx;
        else
            ss << "Caso " << (right ? "RR" : "LL") << ": rotacion simple a la " << other << " en " << nx;
        return ss.str();
    }

    /** Dibuja el árbol resaltando el nodo @p mark. */
    void print(std::ostream& os, const Node* mark = nullptr) const {
        m_mark = mark;
        this->print_tree(os);
        m_mark = nullptr;
    }
};

/** Aplica @p change al árbol y a su copia sin balancear, y muestra el antes y el después del rebalanceo. */
template <typename Tree, typename Change>
void AVLStep(Tree& tree, const std::string& title, Change change) {
    Unbalanced<Tree> sketch(tree);
    change(tree, sketch);
    PrintColor(title, "1;33");
    if (auto x = sketch.unbalanced()) {
        std::cout << "Como quedaria sin balancear:\n";
        sketch.print(std::cout, x);
        PrintColor(sketch.describe(x), "1;31");
        std::cout << "Luego de rebalancear:\n";
    }
    Unbalanced<Tree>(tree).print(std::cout);
    std::cout << '\n';
}

void DemoAVLStepByStep() {
    PrintColor("DemoAVL paso a paso");
    AVLSearchTree<TX> tree;
    for (TX v : {10, 20, 30, 40, 50, 25, 5, 4, 8, 7})
        AVLStep(tree, "insert(" + std::to_string(v) + ")", [v](auto& t, auto& s) {
            s.insert(v, v * 10);
            t.insert(v, v * 10);
        });
    for (TX v : {50, 40, 4, 30})
        AVLStep(tree, "remove(" + std::to_string(v) + ")", [v](auto& t, auto& s) {
            s.remove(v);
            t.remove(v);
        });
    CheckTree(tree, "AVL paso a paso");
}

void DemoHashTableStepByStep() {
    PrintColor("DemoHashTable paso a paso (AVL ordenado por clave)");
    using Table = HashTableOf<std::string, Int>;
    Table table;
    Int age = 18;
    for (std::string key : {"ana", "luis", "eva", "juan", "rosa", "pedro", "maria", "jose"}) {
        std::string title = "tabla[\"" + key + "\"] = " + std::to_string(age);
        AVLStep(table, title, [&](auto& t, auto& s) {
            s.try_insert(key, 0);
            t[key] = age;
        });
        ++age;
    }
    for (std::string key : {"eva", "rosa", "ana"})
        AVLStep(table, "remove(\"" + key + "\")", [&](auto& t, auto& s) {
            s.remove(key);
            t.remove(key);
        });
    std::cout << "Tabla final: " << table << '\n';
}

void DemoRaceCondition() { DemoRaceCondition<VectorOf<TX>>("Vector"); }

void DemoVector() {
    VectorOf<TX> vec;
    Demo(vec, "Vector");

    VectorOf<std::string> strVec;
    TestContainer(strVec, {{"Hello", 1}, {"World", 2}}, "vector_str.txt");
    TestTraversal(strVec, "Vector<string>");

    const auto& constVec = vec;
    CheckReverse("Vector const", constVec, ReverseRange(constVec));
    if (CollectRefs(constVec).size() != vec.size())
        throw std::runtime_error("Vector const: recorridos inconsistentes");

    VectorOf<TX> emptyVector;
    if (emptyVector.rbegin() != emptyVector.rend())
        throw std::runtime_error("Vector vacio: recorrido inverso incorrecto");
    const auto& constEmpty = emptyVector;
    if (constEmpty.begin() != constEmpty.end() || constEmpty.rbegin() != constEmpty.rend())
        throw std::runtime_error("Vector const vacio: recorrido incorrecto");
    std::cout << "Vector: recorrido const y vacio OK\n";
}

void DemoLinkedList() { Demo<SortedLinkedList<TX>, SortedLinkedList<TX, std::greater<TX>>>("LinkedList"); }

void DemoCircularLinkedList() {
    Demo<SortedCircularLinkedList<TX>, SortedCircularLinkedList<TX, std::greater<TX>>>(
        "CircularLinkedList");
}

void DemoDoubleLinkedList() {
    Demo<SortedDoubleLinkedList<TX>, SortedDoubleLinkedList<TX, std::greater<TX>>>("DoubleLinkedList");
}

void DemoCircularDoubleLinkedList() {
    Demo<SortedCircularDoubleLinkedList<TX>,
         SortedCircularDoubleLinkedList<TX, std::greater<TX>>>(
        "CircularDoubleLinkedList");
}

void DemoHeap() {
    Demo<MinHeap<Int>>("MinHeap");
    Demo<MaxHeap<Int>>("MaxHeap");
}

void DemoBinaryTree() { Demo<BinarySearchTree<TX>, BinarySearchTree<TX, std::greater<TX>>>("BinaryTree"); }

void DemoAVL() {
    Demo<AVLSearchTree<TX>, AVLSearchTree<TX, std::greater<TX>>>("AVL");
    DemoRaceCondition<AVLSearchTree<TX>>("AVL");
}

/** Pares (valor, ref) del árbol en orden. */
template <typename Tree> std::vector<std::pair<typename Tree::value_type, Ref>> Dump(const Tree& tree) {
    std::vector<std::pair<typename Tree::value_type, Ref>> out;
    for (auto&& node : tree.InOrder())
        out.emplace_back(node.getValue(), node.getRef());
    return out;
}

/** Guarda @p tree en un archivo, lo lee en un árbol nuevo y exige que tenga exactamente los mismos pares. */
template <typename Tree> Tree RoundTrip(const Tree& tree, const std::string& label, const std::string& file) {
    {
        std::ofstream out(file);
        out << tree;
    }
    Tree loaded;
    {
        std::ifstream in(file);
        in >> loaded;
        if (in.fail()) throw std::runtime_error("BTree persistencia: fallo la lectura de " + label);
    }
    std::remove(file.c_str());
    auto sorted = [](auto d) { std::sort(d.begin(), d.end()); return d; };
    if (sorted(Dump(tree)) != sorted(Dump(loaded)))
        throw std::runtime_error("BTree persistencia: " + label + " no coincide");
    auto d = Dump(loaded);
    typename Tree::Compare comp;
    if (!std::is_sorted(d.begin(), d.end(), [&](const auto& x, const auto& y) { return comp(x.first, y.first); }))
        throw std::runtime_error("BTree persistencia: " + label + " perdio el orden");
    std::cout << "  " << label << ": " << loaded.size() << " elementos OK\n";
    return loaded;
}

/** Prueba lower_bound, upper_bound y range contra un recorrido completo, con y sin duplicados. */
static_assert(NothrowMovable<BTreeOf<TX>>, "BTree: el move debe ser noexcept");
static_assert(OptionalLookup<BTreeOf<TX>> && RangeQueryable<BTreeOf<TX>>, "BTree: faltan find/range");
static_assert(BTreePageConcept<BTreePage<BTreeTraits<TX>>>, "BTree: la pagina no cumple BTreePageConcept");

template <RangeQueryable Tree> void TestRange() {
    PrintColor("TestBTreeRange");
    std::mt19937 rng(777);
    std::vector<TX> keys;
    for (TX i = 0; i < 2000; ++i)
        keys.push_back(i / 2 * 3);
    std::shuffle(keys.begin(), keys.end(), rng);
    Tree tree;
    for (TX k : keys)
        tree.insert(k, k);
    std::sort(keys.begin(), keys.end());

    Tree empty;
    if (empty.lower_bound(0) != std::as_const(empty).end() || empty.upper_bound(0) != std::as_const(empty).end())
        throw std::runtime_error("BTree range: el arbol vacio debe dar end()");

    for (TX probe = -2; probe <= 3003; ++probe) {
        auto lo = std::lower_bound(keys.begin(), keys.end(), probe);
        auto hi = std::upper_bound(keys.begin(), keys.end(), probe);
        auto tl = tree.lower_bound(probe);
        auto tu = tree.upper_bound(probe);
        if ((lo == keys.end()) != (tl == std::as_const(tree).end()) || (hi == keys.end()) != (tu == std::as_const(tree).end()) ||
            (lo != keys.end() && (*tl).getValue() != *lo) || (hi != keys.end() && (*tu).getValue() != *hi))
            throw std::runtime_error("BTree range: lower/upper_bound incorrecto en " + std::to_string(probe));
    }
    for (auto [low, high] : std::vector<std::pair<TX, TX>>{{0, 0}, {-10, 5}, {10, 9}, {100, 400}, {2990, 5000}, {-5, 5000}, {4, 5}}) {
        std::vector<TX> got, expected;
        tree.range(low, high, [&](const auto& node) { got.push_back(node.getValue()); });
        for (TX k : keys)
            if (k >= low && k <= high) expected.push_back(k);
        if (got != expected)
            throw std::runtime_error("BTree range: range(" + std::to_string(low) + "," + std::to_string(high) + ") incorrecto");
    }
    std::cout << "  lower_bound, upper_bound y range coinciden con std::lower_bound/upper_bound OK\n";
}

/** Prueba la persistencia con casos límite: vacío, un elemento, duplicados, negativos, órdenes, comparador, texto inválido. */
void TestBTreePersistence() {
    PrintColor("TestBTreePersistence");
    const std::string file = "btree_persist_test.txt";

    BTreeOf<TX> empty;
    RoundTrip(empty, "vacio", file);

    BTreeOf<TX> one;
    one.insert(42, 7);
    RoundTrip(one, "un elemento", file);

    BTreeOf<TX> dup;
    for (Ref i = 0; i < 20; ++i)
        dup.insert(static_cast<TX>(i % 4), i);
    RoundTrip(dup, "duplicados", file);

    BTreeOf<TX> extremes;
    for (TX k : {std::numeric_limits<TX>::min(), -1, 0, 1, std::numeric_limits<TX>::max()})
        extremes.insert(k, static_cast<Ref>(k));
    RoundTrip(extremes, "negativos y extremos", file);

    std::mt19937 rng(12345);
    std::vector<TX> keys(5000);
    std::iota(keys.begin(), keys.end(), -2500);
    std::shuffle(keys.begin(), keys.end(), rng);
    BTreeOf<TX, std::less<TX>, 3> o3;
    BTreeOf<TX, std::less<TX>, 4> o4;
    BTreeOf<TX, std::less<TX>, 7> o7;
    BTreeOf<TX, std::greater<TX>, 5> desc;
    for (TX k : keys) {
        o3.insert(k, k * 2);
        o4.insert(k, k * 2);
        o7.insert(k, k * 2);
        desc.insert(k, k * 2);
    }
    RoundTrip(o3, "5000 aleatorios orden 3", file);
    RoundTrip(o4, "5000 aleatorios orden 4", file);
    RoundTrip(o7, "5000 aleatorios orden 7", file);
    RoundTrip(desc, "5000 aleatorios descendente", file);

    for (std::size_t i = 0; i < keys.size(); i += 2)
        o3.remove(keys[i]);
    auto afterRemove = RoundTrip(o3, "tras eliminar la mitad", file);
    afterRemove.insert(100000, 1);
    if (o3.contains(100000)) throw std::runtime_error("BTree persistencia: el arbol leido comparte estado");

    auto viaMove = RoundTrip(o4, "copia por move", file);
    BTreeOf<TX, std::less<TX>, 4> moved(std::move(o4));
    if (Dump(moved) != Dump(viaMove)) throw std::runtime_error("BTree persistencia: move cambio el contenido");

    std::stringstream twice;
    twice << o7;
    BTreeOf<TX, std::less<TX>, 7> a;
    twice >> a;
    std::stringstream again;
    again << a;
    if (twice.str() != again.str()) throw std::runtime_error("BTree persistencia: guardar-leer-guardar difiere");
    std::cout << "  guardar-leer-guardar idempotente OK\n";

    BTreeOf<TX> keep;
    keep.insert(1, 10);
    keep.insert(2, 20);
    const auto before = Dump(keep);
    for (const char* bad : {"", "[", "[(1,2)", "[(1,2),]", "(1,2)", "[(a,b)]", "[(1,2)(3,4)]", "[(1,2);(3,4)]", "basura"}) {
        std::istringstream in(bad);
        in >> keep;
        if (!in.fail() || Dump(keep) != before)
            throw std::runtime_error(std::string("BTree persistencia: texto invalido mal manejado: ") + bad);
    }
    std::cout << "  texto invalido: falla y no modifica el arbol OK\n";

    BTreeOf<std::string> words;
    for (const char* w : {"manzana", "pera", "uva", "kiwi", "a b", "x,y", "(p)", "[q]"})
        words.insert(w, static_cast<Ref>(std::strlen(w)));
    RoundTrip(words, "cadenas con espacios, comas y parentesis", file);
}

/** Ejecuta @p workers hilos con @p body(indice) y espera a todos. */
template <typename Body> void RunThreads(Int workers, Body body) {
    std::vector<std::thread> pool;
    for (Int t = 0; t < workers; ++t)
        pool.emplace_back(body, t);
    for (auto& th : pool)
        th.join();
}

/** Prueba la concurrencia del BTree: inserciones disjuntas, try_insert disputado, y escrituras mezcladas con lecturas. */
void TestBTreeRace() {
    PrintColor("TestBTreeRace");
    const TX N = 100000;

    BTreeOf<TX> tree;
    RunThreads(NThreads, [&](Int t) {
        for (TX i = t; i < N; i += NThreads)
            tree.insert(i, i);
    });
    if (tree.size() != static_cast<size_t>(N)) throw std::runtime_error("BTree race: se perdieron inserciones");
    CheckTree(tree, "BTree race insert");
    for (TX i = 0; i < N; ++i)
        if (!tree.contains(i)) throw std::runtime_error("BTree race: falta la clave " + std::to_string(i));
    std::cout << "  " << NThreads << " hilos insertan " << N << " claves disjuntas: ninguna perdida OK\n";

    BTreeOf<TX> unique;
    std::atomic<int> winners{0};
    RunThreads(NThreads, [&](Int) {
        for (TX i = 0; i < 20000; ++i)
            if (unique.try_insert(i, i)) ++winners;
    });
    if (winners != 20000 || unique.size() != 20000)
        throw std::runtime_error("BTree race: try_insert acepto duplicados o perdio claves");
    CheckTree(unique, "BTree race try_insert");
    std::cout << "  try_insert disputado: exactamente un ganador por clave OK\n";

    const TX M = 5000;
    BTreeOf<TX> mixed;
    for (TX i = 0; i < M; ++i)
        mixed.insert(i, i);
    std::atomic<Bool> stop{false};
    std::atomic<std::size_t> reads{0};
    std::vector<std::thread> readers;
    for (int r = 0; r < 2; ++r)
        readers.emplace_back([&] {
            while (!stop) {
                std::size_t count = 0;
                TX previous = std::numeric_limits<TX>::min();
                mixed.call([&](const auto& node) {
                    if (node.getValue() < previous) std::terminate();
                    previous = node.getValue();
                    ++count;
                });
                (void)mixed.contains(static_cast<TX>(count % M));
                ++reads;
            }
        });
    RunThreads(NThreads, [&](Int t) {
        for (TX i = t; i < M; i += NThreads)
            if (i % 2 == 0) mixed.remove(i);
            else mixed.insert(M + i, i);
    });
    stop = true;
    for (auto& th : readers)
        th.join();
    CheckTree(mixed, "BTree race mixto");
    for (TX i = 0; i < M; ++i)
        if (mixed.contains(i) != (i % 2 == 1)) throw std::runtime_error("BTree race: estado final incorrecto");
    if (mixed.size() != static_cast<size_t>(M)) throw std::runtime_error("BTree race: tamano final incorrecto");
    std::cout << "  insert/remove concurrentes con " << reads << " recorridos de lectura OK\n";
}

void DemoBTree() {
    Demo<BTreeOf<TX>, BTreeOf<TX, std::greater<TX>>>("BTree");

    const std::string inserts = "D1XJ2xTg8zKL9AhijOPQcEowRSp0NbW567BUfCqrs4FdtYZakHIuvGV3eMylmn";
    const std::string searches = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const std::string removals = "DYZakHIUwxVJ203ejOP9Qc8AdtuEop1XvTRghSNbW567BfiCqrs4FGMyzKLlmn";

    PrintColor("DemoBTree (caracteres, orden 3)");
    BTreeOf<Char> tree;
    for (std::size_t i = 0; i < inserts.size(); ++i) {
        tree.try_insert(inserts[i], static_cast<Ref>(i * i));
        CheckTree(tree, std::string("BTree insert ") + inserts[i]);
    }
    if (tree.try_insert(inserts[0], 0)) throw std::runtime_error("BTree: try_insert acepto un duplicado");
    std::cout << "size = " << tree.size() << ", height = " << tree.height() << '\n';
    tree.print_tree(std::cout);
    tree.print_tree(std::cout, visual::Layout::Sideways);

    for (Char key : searches) {
        auto ref = tree.find_ref(key);
        if (!ref) throw std::runtime_error(std::string("BTree: no se encontro ") + key);
        std::cout << key << "->" << *ref << ' ';
    }
    std::cout << '\n';

    {
        std::ofstream out("btree.txt");
        out << tree;
    }
    BTreeOf<Char> loaded;
    {
        std::ifstream in("btree.txt");
        in >> loaded;
    }
    std::remove("btree.txt");
    if (loaded.size() != tree.size()) throw std::runtime_error("BTree: la persistencia cambio el tamano");
    for (Char key : searches) {
        auto a = tree.find_ref(key);
        auto b = loaded.find_ref(key);
        if (!b || *a != *b) throw std::runtime_error(std::string("BTree: persistencia perdio ") + key);
    }
    std::cout << "BTree: persistencia (guardar y leer) OK\n";

    for (std::size_t i = 0; i < removals.size(); ++i) {
        if (!tree.remove(removals[i]) || tree.contains(removals[i]))
            throw std::runtime_error(std::string("BTree: no se elimino ") + removals[i]);
        CheckTree(tree, std::string("BTree remove ") + removals[i]);
        if (i == removals.size() / 2) {
            std::cout << "A mitad de las eliminaciones (height = " << tree.height() << "):\n";
            tree.print_tree(std::cout);
        }
    }
    if (!tree.empty() || tree.remove('D')) throw std::runtime_error("BTree: el arbol deberia quedar vacio");
    std::cout << "BTree: inserciones, busquedas y eliminaciones OK\n";

    TestRange<BTreeOf<TX, std::less<TX>, 4>>();
    TestBTreePersistence();
    TestBTreeRace();
    DemoRaceCondition<BTreeOf<TX>>("BTree");
}

/** Árbol de Merkle de cadenas con el hasher por defecto. */
using MerkleStrings = MerkleOf<std::string>;
/** Hasher que usa MerkleStrings. */
using MerkleHasher = StdMerkleHasher<std::string>;

/** Últimos 4 dígitos hexadecimales del hash, para mostrarlo abreviado. */
std::string ShortHash(MerkleStrings::Digest digest) {
    std::ostringstream ss;
    ss << std::hex << std::setw(4) << std::setfill('0') << (digest & 0xffff);
    return ss.str();
}

/** Lanza `std::runtime_error` con el mensaje `nombre: motivo` si @p ok es falso. */
void Check(Bool ok, const std::string& name, const std::string& what) {
    if (!ok) throw std::runtime_error(name + ": " + what);
}

/** Igual que Check() con el nombre `MerkleTree`. */
void CheckMerkle(Bool ok, const std::string& what) { Check(ok, "MerkleTree", what); }

/** Imprime el árbol con su raíz, cantidad de hojas y altura. */
void ShowMerkle(const MerkleStrings& tree, const std::vector<std::string>& blocks) {
    struct View : decltype(tree.tree_view()) {
        const std::vector<std::string>* blocks;
        void label(std::ostream& os, Handle node) const {
            os << (node->is_leaf() ? (*blocks)[node->getRef()] + ":" : "") << ShortHash(node->getValue());
        }
    };
    visual::print_tree(std::cout, View{{tree.tree_view()}, &blocks});
    std::cout << "raiz = " << ShortHash(tree.root_hash()) << ", hojas = " << tree.size()
              << ", altura = " << tree.height() << "\n\n";
}

/** Primeros 8 dígitos del hash en el formato de Bitcoin. */
std::string ShortHash(const Sha256Digest& digest) { return digest.display_hex().substr(0, 8); }

/** Texto con el que se muestra un bloque de tipo cadena. */
std::string BlockText(const std::string& block) { return block; }
/** Texto con el que se muestra un bloque que ya es un hash. */
std::string BlockText(const Sha256Digest& block) { return ShortHash(block); }

/** Lista de bloques de un árbol de Merkle. */
template <MerkleLike Tree> using Blocks = std::vector<typename Tree::value_type>;

/** Construye un árbol de Merkle con los bloques dados. */
template <MerkleLike Tree> Tree BuildMerkle(const Blocks<Tree>& blocks) { return Tree(blocks.begin(), blocks.end()); }

/** Comprueba que cada prueba verifica y que falla con un bloque o una raíz alterados. */
template <MerkleLike Tree>
void TestMerkleProofs(const Tree& tree, const Blocks<Tree>& blocks, const typename Tree::value_type& extra,
                      const std::string& name) {
    PrintColor("TestMerkleProofs (" + name + ")");
    auto root = tree.root_hash();
    auto otherRoot = Tree{extra}.root_hash();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        auto proof = tree.proof(i);
        std::cout << "prueba(" << BlockText(blocks[i]) << ") = " << proof.size() << " hashes\n";
        Check(Tree::verify(blocks[i], proof, root), name, "la prueba de " + BlockText(blocks[i]) + " no verifico");
        Check(!Tree::verify(extra, proof, root), name, "bloque alterado verifico");
        Check(!Tree::verify(blocks[i], proof, otherRoot), name, "raiz alterada verifico");
    }
    std::cout << "Todas las pruebas verifican; bloque o raiz alterados fallan\n";
}

/** Comprueba que `update` equivale a reconstruir el árbol, es reversible y no afecta a las copias. */
template <MerkleLike Tree>
void TestMerkleUpdate(Tree& tree, const Blocks<Tree>& blocks, const typename Tree::value_type& extra,
                      const std::string& name) {
    PrintColor("TestMerkleUpdate (" + name + ")");
    auto before = tree.root_hash();
    Tree snapshot(tree);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        tree.update(i, extra);
        std::cout << "update(" << i << ", " << BlockText(extra) << "): " << ShortHash(before) << " -> "
                  << ShortHash(tree.root_hash()) << '\n';
        Check(tree.root_hash() != before, name, "update no cambio la raiz");
        Check(snapshot.root_hash() == before, name, "la copia no es independiente");
        auto edited = blocks;
        edited[i] = extra;
        Check(BuildMerkle<Tree>(edited).root_hash() == tree.root_hash(), name, "update no coincide con reconstruir");
        tree.update(i, blocks[i]);
        Check(tree.root_hash() == before, name, "restaurar el bloque no restauro la raiz");
    }
    Bool threw = false;
    try { tree.update(blocks.size(), extra); } catch (const std::out_of_range&) { threw = true; }
    Check(threw, name, "update fuera de rango no lanzo");
    std::cout << "update coincide con reconstruir y es reversible\n";
}

/** Prueba el árbol vacío, la copia independiente, `clear` y el árbol de una sola hoja. */
template <MerkleLike Tree> void TestMerkleCopyAndClear(const Blocks<Tree>& blocks, const std::string& name) {
    PrintColor("TestCopyAndClear (" + name + ")");
    Tree tree;
    Bool threw = false;
    try { tree.root_hash(); } catch (const std::logic_error&) { threw = true; }
    Check(tree.empty() && threw, name, "arbol vacio");
    tree = BuildMerkle<Tree>(blocks);
    Tree copy(tree);
    tree.clear();
    Check(tree.empty() && copy.size() == blocks.size(), name, "clear afecto a la copia");
    tree = Tree{blocks[0]};
    Check(tree.height() == 1 && Tree::verify(blocks[0], tree.proof(0), tree.root_hash()), name, "arbol de una hoja");
    std::cout << "clear, copia y arbol de una hoja OK\n";
}

/** Comprueba que añadir bloques uno a uno da la misma raíz y las mismas pruebas que construir de una vez. */
template <MerkleLike Tree>
void TestMerklePushBack(const Blocks<Tree>& blocks, const std::string& name) {
    PrintColor("TestMerklePushBack (" + name + ")");
    Tree tree;
    Blocks<Tree> pushed;
    for (const auto& block : blocks) {
        pushed.push_back(block);
        tree.push_back(block);
        Check(tree.size() == pushed.size(), name, "size tras push_back");
        Check(tree.root_hash() == BuildMerkle<Tree>(pushed).root_hash(), name, "push_back no coincide con reconstruir");
        for (std::size_t i = 0; i < pushed.size(); ++i)
            Check(Tree::verify(pushed[i], tree.proof(i), tree.root_hash()), name, "prueba tras push_back");
    }
    std::cout << "push_back 1.." << blocks.size() << " hojas: raiz y pruebas coinciden con reconstruir\n";
}

/** Comprueba que `<<` y `>>` guardan y recuperan el árbol, y que un texto alterado se rechaza sin cambiar el árbol. */
template <MerkleLike Tree>
void TestMerklePersistence(const Blocks<Tree>& blocks, const std::string& name) {
    PrintColor("TestMerklePersistence (" + name + ")");
    Tree tree = BuildMerkle<Tree>(blocks);
    std::stringstream saved;
    saved << tree;
    Tree loaded;
    saved >> loaded;
    Check(loaded.size() == tree.size() && loaded.root_hash() == tree.root_hash(), name, "el arbol leido no coincide");
    for (std::size_t i = 0; i < blocks.size(); ++i)
        Check(Tree::verify(blocks[i], loaded.proof(i), loaded.root_hash()), name, "prueba del arbol leido");

    std::stringstream none;
    none << Tree{};
    Tree emptied(tree);
    none >> emptied;
    Check(emptied.empty(), name, "leer un arbol vacio no lo vacio");

    std::string text = saved.str();
    Char& digit = text[text.find('(') + 1];
    digit = digit == '9' ? '8' : digit + 1;
    std::istringstream tampered(text);
    Tree target(tree);
    tampered >> target;
    Check(tampered.fail() && target.root_hash() == tree.root_hash(), name, "un texto alterado debio rechazarse");
    std::cout << "escribir y leer " << tree.size() << " hojas OK; un hash alterado se rechaza y el arbol no cambia\n";
}

/** Llama a `update`, `proof` y `root_hash` desde varios hilos y comprueba que el estado final es consistente. */
template <MerkleLike Tree>
void TestMerkleRace(const Blocks<Tree>& blocks, const typename Tree::value_type& extra, const std::string& name) {
    PrintColor("TestRaceCondition (" + name + ")");
    Check(blocks.size() >= std::size_t(NThreads), name, "faltan bloques para la prueba de hilos");
    Tree tree = BuildMerkle<Tree>(blocks);
    std::vector<std::thread> threads;
    for (Int t = 0; t < NThreads; ++t)
        threads.emplace_back([&, t] {
            for (Int i = 0; i < 2000; ++i) {
                tree.update(t, i % 2 ? blocks[t] : extra);
                (void)tree.proof(t);
                (void)tree.root_hash();
            }
        });
    for (auto& thread : threads) thread.join();
    auto edited = blocks;
    for (Int t = 0; t < NThreads; ++t) {
        tree.update(t, extra);
        edited[t] = extra;
    }
    Check(tree.root_hash() == BuildMerkle<Tree>(edited).root_hash(), name,
          "estado inconsistente tras actualizar en paralelo");
    std::cout << NThreads << " hilos con update/proof/root_hash: raiz consistente\n";
}

/** Ejecuta todas las pruebas de Merkle sobre el tipo de árbol indicado. */
template <MerkleLike Tree>
void TestMerkle(const Blocks<Tree>& blocks, const typename Tree::value_type& extra, const std::string& name) {
    Tree tree = BuildMerkle<Tree>(blocks);
    TestMerkleProofs(tree, blocks, extra, name);
    TestMerkleUpdate(tree, blocks, extra, name);
    TestMerklePushBack<Tree>(blocks, name);
    TestMerkleCopyAndClear<Tree>(blocks, name);
    TestMerklePersistence<Tree>(blocks, name);
    TestMerkleRace<Tree>(blocks, extra, name);
}

void DemoMerkleBitcoin() {
    PrintColor("DemoMerkleBitcoin (SHA256 doble, nodo impar duplicado)");
    using Btc = MerkleOf<Sha256Digest, BitcoinMerkleHasher>;
    // bloque 170 de Bitcoin (primera transaccion entre personas)
    Btc block170{Sha256Digest::from_display_hex("b1fea52486ce0c62bb442b530a3f0132b826c74e473d1f2c220bfa78111c5082"),
                 Sha256Digest::from_display_hex("f4184fc596403b9d638783cf57adfe4c75c605f6356fbc91338530e9831e9e16")};
    std::cout << "raiz = " << block170.root_hash().display_hex() << '\n';
    CheckMerkle(block170.root_hash().display_hex() == "7dac2c5666815c17a3b36427de37bb9d2e2c5ccec3f8633eb91a4205cb4c10ff",
                "la raiz del bloque 170 no coincide con Bitcoin");

    Sha256Digest a = Sha256::hash("a"), b = Sha256::hash("b"), c = Sha256::hash("c");
    CheckMerkle(Btc{a, b, c}.root_hash() == Btc{a, b, c, c}.root_hash(), "[a,b,c] debe dar la misma raiz que [a,b,c,c]");
    CheckMerkle(Btc{a}.root_hash() == a, "una sola hoja: la raiz es el txid");
    std::cout << "raiz del bloque 170, duplicado del impar y hoja unica OK\n";

    Blocks<Btc> txids;
    for (Char letter = 'A'; letter <= 'I'; ++letter) txids.push_back(Sha256::hash(std::string(1, letter)));
    TestMerkle<Btc>(txids, Sha256::hash("Z"), "Merkle Bitcoin");
}

void DemoMerkleTree() {
    PrintColor("DemoMerkleTree");
    const std::vector<std::string> blocks{"A", "B", "C", "D", "E"};
    MerkleStrings tree(blocks.begin(), blocks.end());

    PrintColor(tree);
    TestTraversal(tree, "MerkleTree");
    tree.print_tree(std::cout, visual::Layout::Sideways);
    tree.print_tree(std::cout, visual::Layout::Cursor);
    TestTreeTraversals(tree, "MerkleTree");

    TestMerkle<MerkleStrings>({"A", "B", "C", "D", "E", "F", "G", "H", "I"}, "Z", "MerkleTree");
}

void DemoMerkleTreeStepByStep() {
    PrintColor("DemoMerkleTree paso a paso");
    std::vector<std::string> blocks{"A", "B", "C", "D", "E"};
    MerkleStrings tree(blocks.begin(), blocks.end());

    PrintColor("build(A, B, C, D, E): hojas H(bloque), padres H(izq || der), la hoja impar sube", "1;33");
    ShowMerkle(tree, blocks);

    auto before = tree.root_hash();
    blocks[2] = "C2";
    tree.update(2, blocks[2]);
    PrintColor("update(2, \"C2\"): solo cambian los hashes de su camino hasta la raiz", "1;33");
    ShowMerkle(tree, blocks);
    CheckMerkle(before != tree.root_hash(), "update no cambio la raiz");

    const Ref target = 3;
    PrintColor("proof(" + std::to_string(target) + "): hermanos necesarios para llegar a la raiz", "1;33");
    auto hash = MerkleHasher::leaf(blocks[target]);
    std::cout << "h0 = H(" << blocks[target] << ") = " << ShortHash(hash) << '\n';
    Int level = 0;
    for (auto&& step : tree.proof(target)) {
        const auto& s = step.getValue();
        hash = s.sibling_is_right ? MerkleHasher::combine(hash, s.sibling) : MerkleHasher::combine(s.sibling, hash);
        std::string prev = "h" + std::to_string(level++);
        std::cout << "h" << level << " = H("
                  << (s.sibling_is_right ? prev + " || " + ShortHash(s.sibling) : ShortHash(s.sibling) + " || " + prev)
                  << ") = " << ShortHash(hash) << '\n';
    }
    Bool ok = hash == tree.root_hash();
    PrintColor("h" + std::to_string(level) + " == raiz (" + ShortHash(tree.root_hash()) + "): " +
                   (ok ? "verifica" : "NO verifica"),
               ok ? "1;32" : "1;31");
    CheckMerkle(ok, "la prueba paso a paso no verifico");
    CheckMerkle(!MerkleStrings::verify("D2", tree.proof(target), tree.root_hash()), "bloque alterado verifico");
    PrintColor("verify con el bloque alterado \"D2\": no verifica", "1;32");
}

/** Prueba la interfaz de mapa: acceso, búsqueda, inserción, borrado, recorrido, lectura y escritura. */
template <MapLike Map> void TestMap(const std::string& name) {
    auto check = [&](Bool ok, const std::string& what) { Check(ok, name, what); };
    Map edades;
    edades["ana"] = 21;
    edades["luis"] = 25;
    edades["eva"] = 31;
    check(edades["nadie"] == 0 && edades.size() == 4, "operator[] no creo la clave nueva");

    if (auto it = edades.find("luis"); it != edades.end()) it->second += 5;
    check(edades["luis"] == 30, "find no modifico el valor");
    check(edades.erase("nadie") == 1 && edades.erase("nadie") == 0, "erase(clave) fallo");
    check(edades.find("pepe") == edades.end() && !edades.contains("pepe") && edades.contains("ana"),
          "find/contains fallo");

    auto [nuevo, insertado] = edades.insert({"rosa", 40});
    check(insertado && nuevo->first == "rosa" && nuevo->second == 40, "insert fallo");
    check(!edades.insert({"rosa", 99}).second && !edades.emplace("rosa", 99).second && edades["rosa"] == 40,
          "insert/emplace sobrescribio una clave existente");
    check(edades.emplace("beto", 18).second && edades.size() == 5, "emplace fallo");

    std::string claves;
    for (auto&& [clave, edad] : edades) {
        claves += clave + " ";
        ++edad;
    }
    check(claves == "ana beto eva luis rosa " && edades["rosa"] == 41, "recorrido [clave, valor] fallo");
    const auto& lectura = edades;
    Int suma = 0;
    for (const auto& [clave, edad] : lectura) suma += edad;
    for (auto it = lectura.rbegin(); it != lectura.rend(); ++it) suma -= it->second;
    check(suma == 0 && lectura.find("ana")->second == 22 && (*edades.begin()).first == "ana", "recorrido const fallo");

    typename Map::const_iterator fijo = edades.find("beto");
    check(fijo++->first == "beto" && fijo->first == "eva" && lectura.at("eva") == 32, "it++/const_iterator/at fallo");
    edades.at("eva") = 33;
    Bool lanzo = false;
    try {
        edades.at("pepe");
    } catch (const std::out_of_range&) {
        lanzo = true;
    }
    check(lanzo && edades["eva"] == 33 && edades.size() == 5, "at fallo");
    auto siguiente = edades.erase(edades.find("eva"));
    check(siguiente->first == "luis" && edades.erase("rosa") == 1 && edades.size() == 3, "erase(it) fallo");
    check(edades.erase(edades.find("luis")) == edades.end(), "erase del ultimo no devolvio end()");
    check(edades.remove("beto") && !edades.remove("beto") && edades.size() == 1, "remove fallo");

    std::stringstream stream("[(zoe,40),(beto,18),(ana,22)]");
    stream >> edades;
    check(stream && edades.size() == 3 && edades["zoe"] == 40, "operator>> fallo");
    stream.clear();
    stream.str("");
    stream << edades;
    check(stream.str() == "[(ana,22),(beto,18),(zoe,40)]", "operator<< fallo");

    const std::string rara = R"( di "hola", \ (x) [y])";
    edades[rara] = 7;
    stream.clear();
    stream.str("");
    stream << edades;
    std::cout << name << " con clave entre comillas: " << stream.str() << '\n';
    Map leida;
    stream >> leida;
    check(stream && leida.size() == 4 && leida[rara] == 7, "clave con comillas y delimitadores no se recupero");
    std::istringstream sin_cerrar(R"([(ana,1),("abierta,2)])");
    sin_cerrar >> leida;
    check(sin_cerrar.fail() && leida.size() == 4, "comilla sin cerrar debio rechazarse");
    edades.erase(rara);
    TestInvalidRead(edades, name);
    std::cout << name << " leido de texto desordenado: " << edades << '\n';
    edades.print_tree(std::cout);
    std::cout << name << ": interfaz de mapa OK\n";
}

void DemoHashTable() {
    PrintColor("DemoHashTable (interfaz de mapa, clave string)");
    TestMap<HashTableOf<std::string, Int>>("HashTable");
    TestTree<HashTableOf<TX, Ref>::Tree>("HashTable (AVL interno)");
}

void DemoCanvas() {
    PrintColor("DemoCanvas");
    visual::detail::Canvas canvas;
    canvas.label(0, 0, "origen (0,0)", visual::style::head);
    canvas.label(20, 2, "texto en (20,2)", visual::style::value);
    canvas.label(5, 4, "texto en (5,4)", visual::style::loop);
    canvas.hline(0, 30, 6, "─", visual::style::edge);
    canvas.glyph(15, 6, "●", visual::style::end);
    canvas.print(std::cout, visual::Painter(std::cout));
}

void PrintSerializedSheet(const std::string& serialized, std::ostream& os);

void DemoSheet() {
    PrintColor("DemoSheet (hoja de calculo de texto)");
    const std::string name = "Sheet";
    SheetOf<> sheet;
    sheet.set("A1", "10");
    sheet.set("A2", "20");
    sheet.set("A3", "30");
    sheet.set("B1", "=(A1+A2)*2 - -A3/3");
    sheet.set("B2", "=SUMA(A1:A3, 4)");
    sheet.set("B3", "=PROM(A1:A3) + MAX(A1, A2) - MIN(A1:A3) + CONTAR(A1:B2)");
    sheet.set("C1", "Total");
    sheet.set("C2", "=2^3^2");
    sheet.set("C3", "=A1/(A2-20)");
    sheet.set("D1", "=D2");
    sheet.set("D2", "=D1+1");
    sheet.set("D3", "=NADA(1)");
    Check(sheet.value("B1") == 70, name, "parentesis o menos unario");
    Check(sheet.value("b2") == 64, name, "SUMA con rango");
    Check(sheet.value("B3") == 20 + 20 - 10 + 4, name, "PROM/MAX/MIN/CONTAR");
    Check(sheet.value("C2") == 512, name, "^ asocia a la derecha");
    Check(sheet.text("C1") == "Total" && sheet.text("C3") == "#DIV/0" && sheet.text("D1") == "#CICLO" &&
              sheet.text("D3") == "#NAME" && sheet.text("Z9").empty(),
          name, "codigos de error");
    sheet.set("A1", "0");
    Check(sheet.value("B1") == 50, name, "el cambio no se propago");

    sheet.set("G1", "=SI(A2>15, \"Alto\", \"Bajo\")");
    sheet.set("G2", "=CONCAT(C1, \": \", B1)");
    sheet.set("G3", "=LARGO(C1) + SI(A1=0, 10, 0)");
    Check(sheet.text("G1") == "Alto" && sheet.text("G2") == "Total: 50" && sheet.value("G3") == 15, name,
          "SI, comparaciones y cadenas");

    auto postorder = [](const SheetOf<>::Formula& tree) {
        std::string order;
        for (const auto& node : tree.PostOrder()) order += node.getValue().text + " ";
        return order;
    };
    using Formula = SheetOf<>::Formula;
    Check(postorder(Formula("(A1+A2)*2")) == "A1 A2 + 2 * " && postorder(Formula("A1+A2*2")) == "A1 A2 2 * + ",
          name, "los parentesis no cambiaron la forma del AST");
    Check(postorder(Formula("suma(A1:A3, 4, -1)")) == "A1:A3 4 1 - , , SUMA " && Formula("2^3^2").size() == 5 &&
              sheet.formula("C1").empty(),
          name, "AST de funciones o potencias");
    for (std::string bad : {"(1+2", "1 2", "SUMA(A1:)", "A1:A3", "1+#"}) {
        Bool threw = false;
        try {
            Formula tree(bad);
        } catch (const SheetError& error) {
            threw = std::string(error.what()) == "#SYNTAX";
        }
        Check(threw, name, "\"" + bad + "\" debio dar #SYNTAX");
    }
    PrintColor("[1] Tabla evaluada (aritmetica, funciones, SI/CONCAT/LARGO y errores)", "1;33");
    sheet.print(std::cout);

    PrintColor("[2] Persistencia en archivo (SaveTo / LoadFrom -> demo_sheet.txt)", "1;33");
    WithDemoFile("demo_sheet.txt", [&] {
        SaveTo("demo_sheet.txt", sheet);
        SheetOf<> loaded;
        LoadFrom("demo_sheet.txt", loaded);
        std::ostringstream a, b;
        a << sheet;
        b << loaded;
        Check(a.str() == b.str() && loaded.value("B2") == 54 && loaded.text("C1") == "Total", name,
              "el archivo leido no coincide");
        PrintSerializedSheet(a.str(), std::cout);
    });

    PrintColor("[3] Validacion ante entrada invalida (no corrompe la hoja)", "1;33");
    std::istringstream invalid("[(A1,5),(nada,1)]");
    SheetOf<> target;
    target.set("A1", "7");
    invalid >> target;
    Check(invalid.fail() && target.value("A1") == 7, name, "un texto invalido debio rechazarse");
    std::cout << "  Entrada \"[(A1,5),(nada,1)]\" rechazada -> A1 conserva " << target.text("A1") << '\n';

    PrintColor("[4] Concurrencia: 4 hilos escribiendo 200 celdas c/u + lectura simultanea", "1;33");
    std::vector<std::thread> threads;
    for (Int t = 0; t < 4; ++t)
        threads.emplace_back([&, t] {
            for (Int i = 1; i <= 200; ++i) {
                sheet.set("E" + std::to_string(t * 200 + i), "1");
                sheet.value("B3");
            }
        });
    for (auto& thread : threads) thread.join();
    sheet.set("F1", "=SUMA(E1:E800)");
    Check(sheet.value("F1") == 800, name, "escrituras concurrentes perdidas");
    std::cout << "  F1 = SUMA(E1:E800) -> " << sheet.text("F1") << '\n';
    PrintColor(name + ": formulas, archivo y 4 hilos OK", "1;32");
}

/** Imprime el texto serializado `[(celda,texto),...]` agrupado por columna y con color. */
void PrintSerializedSheet(const std::string& serialized, std::ostream& os = std::cout) {
    if (serialized.size() < 2 || serialized.front() != '[' || serialized.back() != ']') {
        os << serialized << '\n';
        return;
    }

    std::vector<std::string> entries;
    Bool in_quotes = false;
    Bool escaped = false;
    Int depth = 0;
    std::size_t start = 1;
    for (std::size_t i = 1; i + 1 < serialized.size(); ++i) {
        Char ch = serialized[i];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (in_quotes) {
            if (ch == '\\') escaped = true;
            else if (ch == '"') in_quotes = false;
            continue;
        }
        if (ch == '"') in_quotes = true;
        else if (ch == '(') ++depth;
        else if (ch == ')') --depth;
        else if (ch == ',' && depth == 0) {
            entries.push_back(serialized.substr(start, i - start));
            start = i + 1;
        }
    }
    if (start < serialized.size() - 1)
        entries.push_back(serialized.substr(start, serialized.size() - 1 - start));

    auto col_of = [](const std::string& entry) {
        std::string col;
        for (std::size_t i = 1; i < entry.size() && std::isalpha(static_cast<Byte>(entry[i])); ++i)
            col += entry[i];
        return col;
    };

    visual::Painter paint(os);
    auto print_entry = [&](const std::string& entry, Bool trailing_comma) {
        std::size_t comma = entry.find(',');
        if (comma == std::string::npos || entry.front() != '(' || entry.back() != ')') {
            os << entry << (trailing_comma ? "," : "");
            return;
        }
        std::string cell = entry.substr(1, comma - 1);
        std::string val = entry.substr(comma + 1, entry.size() - comma - 2);
        Bool is_formula = (!val.empty() && val.front() == '=') || (val.size() >= 2 && val[0] == '"' && val[1] == '=');
        os << paint("2", "(") << paint("1;36", cell) << paint("2", ",")
           << (is_formula ? paint("0;32", val) : val) << paint("2", trailing_comma ? ")," : ")");
    };

    os << paint("2", "[") << '\n';
    std::size_t i = 0;
    while (i < entries.size()) {
        std::string col = col_of(entries[i]);
        std::size_t j = i + 1;
        while (j < entries.size() && col_of(entries[j]) == col) ++j;

        std::size_t line_len = 2;
        for (std::size_t k = i; k < j; ++k) {
            line_len += entries[k].size();
            if (k + 1 < entries.size()) ++line_len;
            if (k + 1 < j) ++line_len;
        }

        if (line_len <= 78) {
            os << "  ";
            for (std::size_t k = i; k < j; ++k) {
                if (k > i) os << ' ';
                print_entry(entries[k], k + 1 < entries.size());
            }
            os << '\n';
        } else {
            for (std::size_t k = i; k < j; ++k) {
                os << (k == i ? "  " : "    ");
                print_entry(entries[k], k + 1 < entries.size());
                os << '\n';
            }
        }
        i = j;
    }
    os << paint("2", "]") << '\n';
}

void DemoSheetStepByStep() {
    PrintColor("DemoSheet paso a paso");
    const std::string name = "Sheet paso a paso";
    SheetOf<> sheet;
    auto step = [&](const std::string& title) {
        PrintColor(title, "1;33");
        sheet.print(std::cout);
    };

    sheet.set("A1", "10");
    sheet.set("A2", "20");
    sheet.set("A3", "30");
    sheet.set("C1", "Notas");
    step("set A1=10, A2=20, A3=30, C1=Notas: numeros y texto se guardan tal cual en la HashTable");

    sheet.set("B1", "=(A1+A2)*2");
    step("B1 = (A1+A2)*2: el parser resuelve primero el parentesis -> (10+20)*2");
    Check(sheet.value("B1") == 60, name, "parentesis");

    auto show = [](const std::string& title, const SheetOf<>::Formula& tree) {
        PrintColor(title, "1;33");
        tree.print_tree(std::cout);
        std::cout << "PostOrder:";
        for (const auto& node : tree.PostOrder()) std::cout << ' ' << node.getValue();
        std::cout << '\n';
    };
    show("AST de B1 = (A1+A2)*2: el parentesis deja el + debajo del *; no genera nodo", sheet.formula("B1"));
    show("AST de A1+A2*2 sin parentesis: ahora el * queda debajo del +", SheetOf<>::Formula("A1+A2*2"));
    PrintColor("Evaluar B1 recorriendo el AST en PostOrder con una pila:", "1;33");
    std::cout << "A1 -> [10]   A2 -> [10, 20]   + -> [30]   2 -> [30, 2]   * -> [60]\n";

    sheet.set("B2", "=SUMA(A1:A3)");
    sheet.set("B3", "=PROM(A1:A3) + MAX(A1, A2) - MIN(A1:A3)");
    step("B2 = SUMA(A1:A3), B3 = PROM(A1:A3) + MAX(A1, A2) - MIN(A1:A3): un rango aporta cada celda numerica");
    Check(sheet.value("B2") == 60 && sheet.value("B3") == 30, name, "funciones con rango");
    show("AST de B3: un rango es una hoja; los argumentos se encadenan con nodos ','", sheet.formula("B3"));

    sheet.set("A1", "40");
    step("A1 = 40: no se guardan resultados, asi que B1, B2 y B3 se recalculan en cascada");
    Check(sheet.value("B1") == 120 && sheet.value("B2") == 90, name, "cascada");

    sheet.set("D1", "=D2");
    sheet.set("D2", "=D1+1");
    sheet.set("D3", "=A1/(A2-20)");
    step("D1 = D2, D2 = D1+1 -> #CICLO;  D3 = A1/(A2-20) -> #DIV/0");
    Check(sheet.text("D1") == "#CICLO" && sheet.text("D3") == "#DIV/0", name, "errores");

    SheetOf<> grades;
    auto gstep = [&](const std::string& title) {
        PrintColor(title, "1;33");
        grades.print(std::cout);
    };
    auto gstatus = [&](const std::string& student_cell, const std::string& prom_cell,
                       const std::string& state_cell, const std::string& summary_cell) {
        std::string st = grades.text(state_cell);
        std::cout << "  " << grades.text(student_cell) << " (prom=" << grades.text(prom_cell) << ") -> ";
        PrintColor(st + " | " + grades.text(summary_cell), st == "Aprobado" ? "1;32" : "1;31");
    };

    grades.set("A1", "Alumno");
    grades.set("B1", "Nota1");
    grades.set("C1", "Nota2");
    grades.set("D1", "Prom");
    grades.set("E1", "Estado");
    grades.set("F1", "Resumen");

    grades.set("A2", "Ana");
    grades.set("B2", "14");
    grades.set("C2", "16.5");
    grades.set("D2", "=PROM(B2:C2)");
    grades.set("E2", "=SI(D2>=11, \"Aprobado\", \"Desaprobado\")");
    grades.set("F2", "=CONCAT(A2, \": \", E2)");

    grades.set("A3", "Luis");
    grades.set("B3", "8");
    grades.set("C3", "11");
    grades.set("D3", "=PROM(B3:C3)");
    grades.set("E3", "=SI(D3>=11, \"Aprobado\", \"Desaprobado\")");
    grades.set("F3", "=CONCAT(A3, \": \", E3, \" (\", LARGO(E3), \")\")");

    gstep("Registro de notas: texto, decimales, PROM, SI y CONCAT");
    gstatus("A2", "D2", "E2", "F2");
    gstatus("A3", "D3", "E3", "F3");
    Check(grades.text("E2") == "Aprobado" && grades.text("E3") == "Desaprobado" && grades.text("F2") == "Ana: Aprobado",
          name, "registro inicial con SI y CONCAT");

    grades.set("C3", "16");
    gstep("Cambio C3 = 16: Luis sube de promedio y pasa a Aprobado en cascada");
    gstatus("A3", "D3", "E3", "F3");
    Check(grades.value("D3") == 12 && grades.text("E3") == "Aprobado", name, "cambio de Desaprobado a Aprobado");

    grades.remove("C3");
    grades.set("G1", "=B2 + 1");
    grades.remove("B2");
    gstep("remove(C3) y remove(B2): en rango PROM ignora vacias; en ref directa G1=B2+1 da #REF");
    gstatus("A2", "D2", "E2", "F2");
    gstatus("A3", "D3", "E3", "F3");
    Check(grades.value("D3") == 8 && grades.text("E3") == "Desaprobado" && grades.text("G1") == "#REF", name,
          "remove en rango y en referencia directa");

    grades.set("G2", "=G3");
    grades.set("G3", "=G2+1");
    grades.set("G4", "=A1+1");
    grades.set("G5", "=NOEXISTE(1)");
    gstep("Errores: G1=#REF, G2/G3=#CICLO, G4=A1+1 (#VALUE por sumar texto), G5=#NAME");
    Check(grades.text("G2") == "#CICLO" && grades.text("G4") == "#VALUE" && grades.text("G5") == "#NAME", name,
          "errores variados");

    PrintColor("operator<<: la HashTable escribe [(celda,texto),...]; las formulas con , o ) van entre comillas", "1;33");
    std::stringstream saved;
    saved << grades;
    PrintSerializedSheet(saved.str());

    SheetOf<> loaded;
    saved >> loaded;
    PrintColor("operator>>: se lee en otra hoja y se recalcula igual", "1;33");
    loaded.print(std::cout);
    Check(saved && loaded.text("E2") == grades.text("E2") && loaded.text("G2") == "#CICLO", name, "lectura");
}

void DemoRegresiones() {
    PrintColor("DemoRegresiones");
    VectorOf<TX> empty;
    Check(empty.rbegin() == empty.rend(), "Regresiones", "un Vector vacio debe tener rbegin() == rend()");

    VectorOf<TX> v;
    v.push_back(1, 0);
    v.push_back(2, 1);
    std::size_t seen = 0;
    v.call([&](const auto&) { seen += v.size(); });
    Check(seen == 4, "Regresiones", "call debe poder consultar el mismo Vector");

    VectorOf<std::string> words;
    words.push_back(std::string(100, 'x'), 0);
    words.pop_back();
    words.push_back("a", 1);
    Check(words.at(0).getValue() == "a" && words.size() == 1, "Regresiones", "pop_back y push_back fallaron");
    std::cout << "Regresiones: OK\n";
}

/** Comprueba una condición de los demos nuevos. @throws std::runtime_error con @p what si falla. */
static void Expect(Bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error(what);
}

/** Pares (valor, ref) del contenedor ordenados, para comparar contenidos sin depender del orden interno. */
template <typename Container> std::vector<std::pair<typename Container::value_type, Ref>> Sorted(const Container& c) {
    std::vector<std::pair<typename Container::value_type, Ref>> items;
    c.call([&](const auto& node) { items.emplace_back(node.getValue(), node.getRef()); });
    std::sort(items.begin(), items.end());
    return items;
}

/** Prueba `lower_bound`, `upper_bound` y `range` del B+ contra un recorrido completo y sigue las hojas enlazadas. */
void TestBPlusRange() {
    PrintColor("TestBPlusRange");
    BPlusTreeOf<TX, std::less<TX>, 4> tree;
    std::mt19937 rng(99);
    std::vector<TX> keys(1500);
    std::iota(keys.begin(), keys.end(), 0);
    std::shuffle(keys.begin(), keys.end(), rng);
    for (TX k : keys)
        tree.insert(k * 3, k);
    Expect(tree.validate(), "B+: arbol invalido tras insertar");
    for (TX probe = -2; probe <= 4505; ++probe) {
        auto lo = tree.lower_bound(probe), hi = tree.upper_bound(probe);
        TX expectLo = probe <= 0 ? 0 : (probe + 2) / 3 * 3, expectHi = probe < 0 ? 0 : (probe / 3 + 1) * 3;
        Expect((expectLo > 4497) == (lo == std::as_const(tree).end()) && (expectLo > 4497 || (*lo).getValue() == expectLo),
               "B+: lower_bound incorrecto en " + std::to_string(probe));
        Expect((expectHi > 4497) == (hi == std::as_const(tree).end()) && (expectHi > 4497 || (*hi).getValue() == expectHi),
               "B+: upper_bound incorrecto en " + std::to_string(probe));
    }
    std::vector<TX> got;
    std::size_t visited = tree.range(100, 130, [&](const auto& node) { got.push_back(node.getValue()); });
    Expect(visited == 10 && got.front() == 102 && got.back() == 129 && std::is_sorted(got.begin(), got.end()),
           "B+: range(100,130) incorrecto");
    Expect(tree.range(5000, 6000, [](const auto&) {}) == 0 && tree.range(130, 100, [](const auto&) {}) == 0,
           "B+: range vacio incorrecto");
    std::cout << "  " << tree.leaf_count() << " hojas enlazadas, altura " << tree.height()
              << ", range(100,130) visito " << visited << " claves OK\n";
}

/** Inserta y elimina al azar comparando con un std::map y validando las invariantes cada cierto tiempo. */
template <std::size_t Order> void TestBPlusRandom() {
    BPlusTreeOf<TX, std::less<TX>, Order> tree;
    std::map<TX, Ref> model;
    std::mt19937 rng(Order);
    for (int i = 0; i < 20000; ++i) {
        TX key = rng() % 1500;
        if (rng() % 3) Expect(tree.insert(key, key * 2) == model.emplace(key, key * 2).second, "B+: insert difiere del modelo");
        else Expect(tree.remove(key) == (model.erase(key) > 0), "B+: remove difiere del modelo");
        if (i % 400 == 0) Expect(tree.validate(), "B+: invariantes rotas");
    }
    Expect(tree.validate() && tree.size() == model.size(), "B+: tamano difiere del modelo");
    auto expected = model.begin();
    for (auto&& node : tree) {
        Expect(node.getValue() == expected->first && node.getRef() == expected->second, "B+: recorrido difiere del modelo");
        ++expected;
    }
    auto reverse = model.rbegin();
    for (auto it = tree.rbegin(); it != tree.rend(); ++it, ++reverse)
        Expect((*it).getValue() == reverse->first, "B+: recorrido inverso difiere del modelo");
    for (auto& [key, ref] : model)
        Expect(tree.remove(key), "B+: no se pudo vaciar");
    Expect(tree.empty() && tree.validate() && tree.begin() == tree.end(), "B+: deberia quedar vacio");
    std::cout << "  orden " << Order << ": 20000 operaciones aleatorias contra std::map OK\n";
}

/** Persistencia del B+: texto, archivo, registros `id valor`, copia, move y texto inválido. */
void TestBPlusPersistence() {
    PrintColor("TestBPlusPersistence");
    BPlusTreeOf<TX> empty;
    BPlusTreeOf<TX, std::less<TX>, 3> small;
    BPlusTreeOf<TX, std::less<TX>, 5> big;
    for (TX k : {42}) small.insert(k, 7);
    std::mt19937 rng(5);
    for (int i = 0; i < 3000; ++i)
        big.insert(static_cast<TX>(rng() % 100000) - 50000, i);
    auto roundTrip = [](const auto& tree, const std::string& label) {
        std::stringstream text;
        text << tree;
        std::remove_cvref_t<decltype(tree)> loaded;
        text >> loaded;
        Expect(!text.fail() && Sorted(tree) == Sorted(loaded) && loaded.validate(), "B+ persistencia: " + label);
        tree.save("bplus_persist.txt");
        std::remove_cvref_t<decltype(tree)> fromFile;
        fromFile.load("bplus_persist.txt");
        tree.save_records("bplus_records.txt");
        std::remove_cvref_t<decltype(tree)> fromRecords;
        fromRecords.load_records("bplus_records.txt");
        std::remove("bplus_persist.txt");
        std::remove("bplus_records.txt");
        Expect(Sorted(tree) == Sorted(fromFile) && Sorted(tree) == Sorted(fromRecords) && fromFile.validate(),
               "B+ persistencia por archivo: " + label);
        std::cout << "  " << label << ": " << tree.size() << " elementos (texto, archivo y registros) OK\n";
    };
    roundTrip(empty, "vacio");
    roundTrip(small, "un elemento");
    roundTrip(big, "3000 aleatorios, orden 5");

    BPlusTreeOf<TX, std::less<TX>, 5> copy(big), moved(std::move(copy));
    Expect(Sorted(moved) == Sorted(big) && copy.empty() && moved.validate(), "B+: copia o move incorrectos");
    static_assert(NothrowMovable<BPlusTreeOf<TX>> && OptionalLookup<BPlusTreeOf<TX>> && RangeQueryable<BPlusTreeOf<TX>>);

    BPlusTreeOf<TX> keep{{1, 10}, {2, 20}};
    const auto before = Sorted(keep);
    for (const char* bad : {"", "[", "[(1,2)", "[(1,2),]", "(1,2)", "[(a,b)]", "basura"}) {
        std::istringstream in(bad);
        in >> keep;
        Expect(in.fail() && Sorted(keep) == before, std::string("B+: texto invalido mal manejado: ") + bad);
    }
    std::cout << "  texto invalido: falla y no modifica el arbol OK\n";
}

/** Concurrencia del B+: inserciones disjuntas, inserción disputada y escritores con lectores que recorren. */
void TestBPlusRace() {
    PrintColor("TestBPlusRace");
    const TX N = 50000;
    BPlusTreeOf<TX> tree;
    RunThreads(NThreads, [&](Int t) {
        for (TX i = t; i < N; i += NThreads)
            tree.insert(i, i);
    });
    Expect(tree.size() == static_cast<std::size_t>(N) && tree.validate(), "B+ race: se perdieron inserciones");
    std::cout << "  " << NThreads << " hilos insertan " << N << " claves disjuntas: ninguna perdida OK\n";

    BPlusTreeOf<TX> unique;
    std::atomic<int> winners{0};
    RunThreads(NThreads, [&](Int) {
        for (TX i = 0; i < 10000; ++i)
            if (unique.insert(i, i)) ++winners;
    });
    Expect(winners == 10000 && unique.size() == 10000 && unique.validate(), "B+ race: insert acepto duplicados");
    std::cout << "  insert disputado: exactamente un ganador por clave OK\n";

    const TX M = 4000;
    BPlusTreeOf<TX> mixed;
    for (TX i = 0; i < M; ++i)
        mixed.insert(i, i);
    std::atomic<Bool> stop{false};
    std::atomic<std::size_t> reads{0};
    std::vector<std::thread> readers;
    for (int r = 0; r < 2; ++r)
        readers.emplace_back([&] {
            while (!stop) {
                TX previous = std::numeric_limits<TX>::min();
                mixed.call([&](const auto& node) {
                    if (node.getValue() <= previous) std::terminate();
                    previous = node.getValue();
                });
                (void)mixed.range(100, 200, [](const auto&) {});
                ++reads;
            }
        });
    RunThreads(NThreads, [&](Int t) {
        for (TX i = t; i < M; i += NThreads)
            if (i % 2 == 0) mixed.remove(i);
            else mixed.insert(M + i, i);
    });
    stop = true;
    for (auto& th : readers)
        th.join();
    Expect(mixed.validate() && mixed.size() == static_cast<std::size_t>(M), "B+ race: estado final incorrecto");
    for (TX i = 0; i < M; ++i)
        Expect(mixed.contains(i) == (i % 2 == 1), "B+ race: contenido final incorrecto");
    std::cout << "  insert/remove concurrentes con " << reads << " recorridos de lectura OK\n";
}

void DemoBPlusTree() {
    PrintColor("DemoBPlusTree");
    BPlusTreeOf<TX> container;
    TestContainer(container, Values<BPlusTreeOf<TX>>{{5, 50}, {1, 10}, {4, 40}, {2, 20}, {7, 70}, {3, 30}, {9, 90}, {8, 80}},
                  "BPlusTree.txt");
    TestTraversal(container, "BPlusTree");
    TestVisualize(container, "BPlusTree");
    container.clear();
    TestGenericContainer(container, "BPlusTree_persist.txt");
    TestInvalidRead(container, "BPlusTree");
    BPlusTreeOf<TX> copyAndClear;
    TestCopyAndClear(copyAndClear, "BPlusTree");
    TestApplyFunction(container);

    PrintColor("DemoBPlusTree (enteros, orden 3)");
    BPlusTreeOf<TX, std::less<TX>, 3> tree;
    for (TX k = 1; k <= 20; ++k) {
        Expect(tree.insert(k, k * k), "B+: insert fallo");
        Expect(tree.validate(), "B+: invariantes rotas al insertar " + std::to_string(k));
    }
    Expect(!tree.insert(5, 0), "B+: acepto un duplicado");
    std::cout << "size = " << tree.size() << ", height = " << tree.height() << ", hojas = " << tree.leaf_count() << '\n';
    tree.print_tree(std::cout);
    for (TX k : {1, 2, 3, 20, 19, 10, 11, 12})
        Expect(tree.remove(k) && tree.validate(), "B+: remove fallo en " + std::to_string(k));
    std::cout << "Tras eliminar 8 claves:\n";
    tree.print_tree(std::cout);
    PrintColor(tree);

    TestBPlusRange();
    PrintColor("TestBPlusRandom");
    TestBPlusRandom<3>();
    TestBPlusRandom<4>();
    TestBPlusRandom<7>();
    TestBPlusPersistence();
    TestBPlusRace();
    DemoRaceCondition<BPlusTreeOf<TX>>("BPlusTree");
    std::cout << "BPlusTree: OK\n";
}

/** Rejilla de @p side x @p side vértices con aristas entre vecinos y un muro con una sola abertura. */
GraphOf<std::string> MakeGridGraph(int side) {
    GraphOf<std::string> graph;
    auto name = [&](int x, int y) { return std::to_string(y * side + x); };
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x)
            graph.add_vertex(name(x, y), x, y);
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
            if (x + 1 < side) graph.add_edge(name(x, y), name(x + 1, y));
            if (y + 1 < side) graph.add_edge(name(x, y), name(x, y + 1));
        }
    for (int y = 0; y < side; ++y)
        if (y != side - 2) graph.remove_edge(name(side / 2, y), name(side / 2 + 1, y));
    return graph;
}

/** Imprime el camino como `a -> b -> c` con su costo. */
template <typename Path> void PrintPath(const std::string& label, const Path& path) {
    std::cout << label << ": ";
    if (!path.found) {
        std::cout << "sin camino (expandio " << path.expanded << ")\n";
        return;
    }
    for (std::size_t i = 0; i < path.vertices.size(); ++i)
        std::cout << (i ? " -> " : "") << path.vertices[i].getValue();
    std::cout << "  [costo " << path.cost << ", expandidos " << path.expanded << "]\n";
}

/** Búsqueda en anchura y profundidad en un grafo pequeño, con el orden de visita esperado. */
void TestGraphTraversals() {
    PrintColor("TestGraphTraversals");
    GraphOf<std::string> graph;
    for (const char* name : {"A", "B", "C", "D", "E", "F"})
        graph.add_vertex(name);
    for (auto [a, b] : {std::pair{"A", "B"}, {"A", "C"}, {"B", "D"}, {"C", "D"}, {"D", "E"}})
        graph.add_edge(a, b);
    graph.visualize(std::cout);
    std::string bfs, dfs;
    std::size_t reached = graph.bfs("A", [&](const std::string& key) { bfs += key; });
    graph.dfs("A", [&](const std::string& key) { dfs += key; });
    std::cout << "BFS desde A: " << bfs << ", DFS desde A: " << dfs << ", F no se alcanza\n";
    Expect(bfs == "ABCDE" && dfs == "ABDCE" && reached == 5, "Graph: recorridos incorrectos");
    Expect(graph.degree("D") == 3 && graph.degree("F") == 0 && !graph.has_edge("A", "D") && graph.has_edge("D", "E"),
           "Graph: grados o aristas incorrectos");
    Expect(graph.weight("A", "B") == 1 && !graph.weight("A", "E"), "Graph: peso incorrecto");
    Expect(graph.remove_edge("A", "B") && !graph.has_edge("B", "A") && !graph.remove_edge("A", "B"), "Graph: remove_edge");
    try {
        graph.add_edge("A", "Z");
        Expect(false, "Graph: acepto un vertice inexistente");
    } catch (const std::out_of_range&) {
    }
    try {
        graph.add_edge("A", "B", -1);
        Expect(false, "Graph: acepto un peso negativo");
    } catch (const std::invalid_argument&) {
    }
}

/** A* contra Dijkstra y contra una búsqueda de fuerza bruta (Bellman-Ford) en un grafo aleatorio con pesos. */
void TestGraphAStar() {
    PrintColor("TestGraphAStar");
    GraphOf<std::string> grid = MakeGridGraph(15);
    auto astar = grid.shortest_path("0", "224"), dijkstra = grid.dijkstra("0", "224");
    PrintPath("A*      ", astar);
    PrintPath("Dijkstra", dijkstra);
    Expect(astar.found && astar.cost == dijkstra.cost && astar.expanded <= dijkstra.expanded,
           "Graph: A* debe dar el mismo costo expandiendo menos");

    std::mt19937 rng(2024);
    const int n = 60;
    GraphOf<int, Real> graph;
    std::vector<std::vector<Real>> dist(n, std::vector<Real>(n, std::numeric_limits<Real>::infinity()));
    for (int i = 0; i < n; ++i) {
        graph.add_vertex(i, rng() % 100, rng() % 100);
        dist[i][i] = 0;
    }
    for (int i = 0; i < 200; ++i) {
        int a = rng() % n, b = rng() % n;
        if (a == b) continue;
        Real w = 1 + rng() % 50;
        graph.add_edge(a, b, w, true);
        dist[a][b] = w;
    }
    for (int k = 0; k < n; ++k)
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                dist[i][j] = std::min(dist[i][j], dist[i][k] + dist[k][j]);
    int checked = 0;
    for (int a = 0; a < n; ++a)
        for (int b = 0; b < n; ++b) {
            auto path = graph.dijkstra(a, b);
            Expect(path.found == (dist[a][b] < std::numeric_limits<Real>::infinity()), "Graph: alcance incorrecto");
            if (path.found) {
                Expect(path.cost == dist[a][b], "Graph: Dijkstra difiere de Floyd-Warshall");
                ++checked;
            }
        }
    std::cout << "  " << checked << " caminos comparados con Floyd-Warshall OK\n";

    GraphOf<std::string> isolated;
    isolated.add_vertex("a");
    isolated.add_vertex("b");
    Expect(!isolated.shortest_path("a", "b").found && isolated.shortest_path("a", "a").cost == 0, "Graph: sin camino");
}

/** Persistencia del grafo: texto, archivo, claves con caracteres especiales, texto inválido y copia. */
void TestGraphPersistence() {
    PrintColor("TestGraphPersistence");
    GraphOf<std::string> graph = MakeGridGraph(8);
    graph.add_vertex("a, (b)", 2.5, -1, 3);
    graph.add_edge("a, (b)", "0", 4.5);
    graph.add_edge("0", "63", 2, true);
    std::stringstream text;
    text << graph;
    GraphOf<std::string> loaded;
    text >> loaded;
    Expect(!text.fail() && loaded.vertex_count() == graph.vertex_count() && loaded.edge_count() == graph.edge_count(),
           "Graph persistencia: tamanos");
    Expect(loaded.weight("a, (b)", "0") == 4.5 && loaded.weight("0", "63") == 2 && !loaded.weight("63", "0"),
           "Graph persistencia: pesos o direccion");
    Expect(loaded.dijkstra("0", "63").cost == graph.dijkstra("0", "63").cost, "Graph persistencia: caminos");
    graph.save("graph_persist.txt");
    GraphOf<std::string> fromFile;
    fromFile.load("graph_persist.txt");
    std::remove("graph_persist.txt");
    std::stringstream again;
    again << fromFile;
    Expect(again.str() == text.str() || fromFile.edge_count() == graph.edge_count(), "Graph persistencia: archivo");
    std::cout << "  " << loaded.vertex_count() << " vertices y " << loaded.edge_count() << " aristas OK\n";

    GraphOf<std::string> keep;
    keep.add_vertex("x");
    for (const char* bad : {"", "GRAPH", "GRAPH 1", "GRAPH 1 0\n", "GRAPH 1 0\n(a,0) 0 0", "GRAPH 2 1\n(a,0) 0 0 0\n(b,1) 1 1 1\n0 5 1",
                            "GRAPH 2 0\n(a,0) 0 0 0\n(a,1) 1 1 1", "GRAPH 1 1\n(a,0) 0 0 0\n0 0 -3", "basura"}) {
        std::istringstream in(bad);
        in >> keep;
        Expect(in.fail() && keep.vertex_count() == 1 && keep.has_vertex("x"),
               std::string("Graph: texto invalido mal manejado: ") + bad);
    }
    std::cout << "  texto invalido: falla y no modifica el grafo OK\n";
    Bool rejected = false;
    try {
        keep.load("no_existe.txt");
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Expect(rejected, "Graph: abrio un archivo inexistente");
}

/** Concurrencia del grafo: escritores agregan vértices y aristas mientras otros hilos buscan caminos y recorren. */
void TestGraphRace() {
    PrintColor("TestGraphRace");
    GraphOf<int> graph;
    const int perThread = 400;
    for (int i = 0; i < NThreads * perThread; ++i)
        graph.add_vertex(i, i % 50, i / 50);
    std::atomic<Bool> stop{false};
    std::atomic<std::size_t> searches{0};
    std::vector<std::thread> readers;
    for (int r = 0; r < 2; ++r)
        readers.emplace_back([&] {
            while (!stop) {
                (void)graph.shortest_path(0, NThreads * perThread - 1);
                graph.bfs(0, [](int) {});
                ++searches;
            }
        });
    RunThreads(NThreads, [&](Int t) {
        for (int i = t * perThread; i + 1 < (t + 1) * perThread; ++i)
            graph.add_edge(i, i + 1);
    });
    for (int t = 0; t + 1 < NThreads; ++t)
        graph.add_edge((t + 1) * perThread - 1, (t + 1) * perThread);
    stop = true;
    for (auto& th : readers)
        th.join();
    const int total = NThreads * perThread;
    Expect(graph.edge_count() == static_cast<std::size_t>(2 * (total - 1)), "Graph race: se perdieron aristas");
    auto path = graph.dijkstra(0, total - 1);
    Expect(path.found && path.cost == total - 1, "Graph race: camino final incorrecto");
    std::cout << "  " << NThreads << " hilos agregan " << graph.edge_count() << " aristas dirigidas mientras otros hacen "
              << searches << " busquedas OK\n";
}

void DemoGraph() {
    PrintColor("DemoGraph");
    TestGraphTraversals();
    TestGraphAStar();
    TestGraphPersistence();
    TestGraphRace();

    PrintColor("DemoGraph (visualizacion VTK)");
    GraphOf<std::string> grid = MakeGridGraph(15);
    auto path = grid.shortest_path("0", "224");
    grid.save_vtk("graph.vtk", &path);
    std::ifstream vtk("graph.vtk");
    std::string header;
    std::getline(vtk, header);
    Expect(header.rfind("# vtk DataFile", 0) == 0, "Graph: VTK sin encabezado");
    std::cout << "Escribio graph.vtk (" << grid.vertex_count() << " puntos, camino de " << path.vertices.size()
              << " vertices en `on_path`); abrirlo con ParaView\n";
    std::cout << "Graph: OK\n";
}

/** Rectángulo aleatorio dentro de un área de 1000x1000. */
Rect<Real> RandomRect(std::mt19937& rng) {
    Real x = rng() % 1000, y = rng() % 1000;
    return {x, y, x + rng() % 30, y + rng() % 30};
}

/** Busca ventanas y el vecino más cercano en el árbol R y lo compara con una búsqueda lineal. */
template <typename Tree> void CheckRTreeQueries(const Tree& tree, const std::vector<std::pair<Rect<Real>, Ref>>& all,
                                                std::mt19937& rng, const std::string& label) {
    for (int q = 0; q < 150; ++q) {
        Rect<Real> window = RandomRect(rng);
        window.maxx += 80, window.maxy += 80;
        std::set<Ref> got, expected;
        std::size_t found = tree.search(window, [&](const auto&, Ref ref) { got.insert(ref); });
        for (auto& [rect, ref] : all)
            if (rect.intersects(window)) expected.insert(ref);
        Expect(got == expected && found == expected.size(), "RTree: ventana incorrecta (" + label + ")");
        Real x = rng() % 1000, y = rng() % 1000, best = std::numeric_limits<Real>::infinity();
        for (auto& [rect, ref] : all)
            best = std::min(best, rect.distance2(x, y));
        auto nearest = tree.nearest(x, y);
        Expect(all.empty() ? !nearest : nearest && nearest->getValue().distance2(x, y) == best,
               "RTree: vecino mas cercano incorrecto (" + label + ")");
    }
}

/** Inserción, eliminación y consultas del árbol R contra una búsqueda lineal. */
template <std::size_t Order> void TestRTreeRandom() {
    RTreeOf<Real, Order> tree;
    std::vector<std::pair<Rect<Real>, Ref>> all;
    std::mt19937 rng(Order);
    for (Ref i = 0; i < 2000; ++i) {
        all.emplace_back(RandomRect(rng), i);
        tree.insert(all.back().first, i);
        if (i % 250 == 0) Expect(tree.validate(), "RTree: invariantes rotas al insertar");
    }
    Expect(tree.validate() && tree.size() == 2000, "RTree: tamano tras insertar");
    CheckRTreeQueries(tree, all, rng, "orden " + std::to_string(Order));
    std::shuffle(all.begin(), all.end(), rng);
    for (std::size_t i = 0; i < 1500; ++i) {
        Expect(tree.remove(all.back().first, all.back().second), "RTree: no se pudo eliminar");
        all.pop_back();
        if (i % 150 == 0) Expect(tree.validate() && tree.size() == all.size(), "RTree: invariantes rotas al eliminar");
    }
    Expect(!tree.remove({-5, -5, -4, -4}, 0), "RTree: elimino algo inexistente");
    CheckRTreeQueries(tree, all, rng, "tras eliminar");
    for (auto& [rect, ref] : all)
        Expect(tree.remove(rect, ref), "RTree: no se pudo vaciar");
    Expect(tree.empty() && tree.validate() && !tree.nearest(0, 0) && tree.count({0, 0, 1000, 1000}) == 0,
           "RTree: deberia quedar vacio");
    std::cout << "  orden " << Order << ": 2000 inserciones, 2000 eliminaciones y consultas contra busqueda lineal OK\n";
}

/** Persistencia del árbol R: texto, archivo, vacío, copia, move y texto inválido. */
void TestRTreePersistence() {
    PrintColor("TestRTreePersistence");
    RTreeOf<Real, 4> tree;
    std::mt19937 rng(8);
    for (Ref i = 0; i < 500; ++i)
        tree.insert(RandomRect(rng), i);
    tree.insert_point(0.5, -2.25, 999);
    auto entries = [](const auto& t) {
        std::vector<std::pair<std::string, Ref>> items;
        t.search({-1e9, -1e9, 1e9, 1e9}, [&](const auto& r, Ref f) {
            std::ostringstream os;
            os << r;
            items.emplace_back(os.str(), f);
        });
        std::sort(items.begin(), items.end());
        return items;
    };
    std::stringstream text;
    text << tree;
    RTreeOf<Real, 4> loaded;
    text >> loaded;
    Expect(!text.fail() && entries(loaded) == entries(tree) && loaded.validate(), "RTree persistencia: texto");
    tree.save("rtree_persist.txt");
    RTreeOf<Real, 4> fromFile;
    fromFile.load("rtree_persist.txt");
    std::remove("rtree_persist.txt");
    Expect(entries(fromFile) == entries(tree), "RTree persistencia: archivo");
    RTreeOf<Real, 4> empty, emptyLoaded;
    std::stringstream emptyText;
    emptyText << empty;
    emptyText >> emptyLoaded;
    Expect(!emptyText.fail() && emptyLoaded.empty(), "RTree persistencia: vacio");
    RTreeOf<Real, 4> copy(tree), moved(std::move(copy));
    Expect(entries(moved) == entries(tree) && copy.empty() && moved.validate(), "RTree: copia o move");
    std::cout << "  " << tree.size() << " rectangulos (texto y archivo), vacio, copia y move OK\n";

    RTreeOf<Real, 4> keep{{{0, 0, 1, 1}, 1}};
    for (const char* bad : {"", "RTREE", "RTREE 1", "RTREE 1\n(0 0 1 1,", "RTREE 1\n(5 5 1 1,3)", "RTREE 2\n(0 0 1 1,1)",
                            "TREE 0", "basura"}) {
        std::istringstream in(bad);
        in >> keep;
        Expect(in.fail() && keep.size() == 1 && keep.contains({0, 0, 1, 1}, 1),
               std::string("RTree: texto invalido mal manejado: ") + bad);
    }
    std::cout << "  texto invalido: falla y no modifica el arbol OK\n";
}

/** Concurrencia del árbol R: inserciones disjuntas, eliminaciones y lectores que buscan ventanas a la vez. */
void TestRTreeRace() {
    PrintColor("TestRTreeRace");
    const Ref N = 10000;
    RTreeOf<Real, 6> tree;
    RunThreads(NThreads, [&](Int t) {
        std::mt19937 rng(t);
        for (Ref i = t; i < N; i += NThreads)
            tree.insert(RandomRect(rng), i);
    });
    Expect(tree.size() == static_cast<std::size_t>(N) && tree.validate(), "RTree race: se perdieron inserciones");
    Expect(tree.count({-1, -1, 2000, 2000}) == static_cast<std::size_t>(N), "RTree race: busqueda incompleta");
    std::cout << "  " << NThreads << " hilos insertan " << N << " rectangulos: ninguno perdido OK\n";

    std::atomic<Bool> stop{false};
    std::atomic<std::size_t> searches{0};
    std::vector<std::thread> readers;
    for (int r = 0; r < 2; ++r)
        readers.emplace_back([&, r] {
            std::mt19937 rng(100 + r);
            while (!stop) {
                (void)tree.count({Real(rng() % 900), Real(rng() % 900), Real(rng() % 900 + 100), Real(rng() % 900 + 100)});
                (void)tree.nearest(rng() % 1000, rng() % 1000);
                ++searches;
            }
        });
    RunThreads(NThreads, [&](Int t) {
        std::mt19937 rng(t + 50);
        for (Ref i = 0; i < 400; ++i)
            tree.insert(RandomRect(rng), N + t * 400 + i);
    });
    stop = true;
    for (auto& th : readers)
        th.join();
    Expect(tree.size() == static_cast<std::size_t>(N + NThreads * 400) && tree.validate(), "RTree race: estado final");
    std::cout << "  inserciones concurrentes con " << searches << " busquedas de lectura OK\n";
}

void DemoRTree() {
    PrintColor("DemoRTree");
    RTreeOf<Real, 4> tree;
    std::vector<std::pair<Rect<Real>, Ref>> all;
    const Rect<Real> shapes[] = {{0, 0, 2, 2},   {1, 1, 3, 3},   {5, 5, 6, 6},   {7, 1, 9, 3},   {2, 6, 4, 8},
                                 {8, 8, 10, 10}, {4, 0, 5, 1},   {6, 3, 7, 4},   {0, 8, 1, 9},   {9, 5, 10, 6},
                                 {3, 3, 4, 4},   {5, 8, 6, 9}};
    for (Ref i = 0; i < static_cast<Ref>(std::size(shapes)); ++i) {
        tree.insert(shapes[i], i);
        all.emplace_back(shapes[i], i);
        Expect(tree.validate(), "RTree: invariantes rotas al insertar " + std::to_string(i));
    }
    std::cout << "size = " << tree.size() << ", height = " << tree.height() << ", bounds = [" << tree.bounds() << "]\n";
    tree.print_tree(std::cout);
    std::cout << "Entradas que tocan [0,0,3,3]: ";
    tree.search({0, 0, 3, 3}, [](const auto& rect, Ref ref) { std::cout << ref << "=(" << rect << ") "; });
    std::cout << "\nMas cercano a (5.4, 4.8): " << *tree.nearest(5.4, 4.8) << '\n';
    Expect(tree.count({0, 0, 3, 3}) == 3 && tree.nearest(5.4, 4.8)->getRef() == 2, "RTree: consultas del ejemplo");

    TestRTreeRandom<4>();
    TestRTreeRandom<5>();
    TestRTreeRandom<9>();
    TestRTreePersistence();
    TestRTreeRace();

    PrintColor("DemoRTree (visualizacion VTK)");
    RTreeOf<Real, 5> big;
    std::mt19937 rng(11);
    for (Ref i = 0; i < 400; ++i)
        big.insert(RandomRect(rng), i);
    big.save_vtk("rtree.vtk", 5);
    std::ifstream vtk("rtree.vtk");
    std::string header;
    std::getline(vtk, header);
    Expect(header.rfind("# vtk DataFile", 0) == 0, "RTree: VTK sin encabezado");
    std::cout << "Escribio rtree.vtk (" << big.size() << " rectangulos, " << big.height()
              << " niveles apilados en z); abrirlo con ParaView\n";
    std::cout << "RTree: OK\n";
}

/** Guarda y lee @p container con los dos formatos de archivo y comprueba que el contenido no cambia. */
template <typename Container> void TestFileRoundTrip(const std::string& name, const Values<Container>& values) {
    Container original;
    for (const auto& [value, ref] : values)
        original.push_back(value, ref);
    const std::string bracket = "files_" + name + ".txt", records = "files_" + name + ".rec";
    original.save(bracket);
    original.save_records(records);
    Container fromBracket, fromRecords;
    fromBracket.load(bracket);
    std::size_t count = fromRecords.load_records(records);
    std::remove(bracket.c_str());
    std::remove(records.c_str());
    Expect(Sorted(original) == Sorted(fromBracket), name + ": save/load cambio el contenido");
    Expect(Sorted(original) == Sorted(fromRecords) && count == values.size(), name + ": save_records/load_records cambio el contenido");
    std::cout << "  " << name << ": " << count << " elementos por save/load y save_records/load_records OK\n";
}

/** Escribe @p text en @p path. */
static void WriteText(const std::string& path, const std::string& text) {
    std::ofstream(path, std::ios::trunc) << text;
}

void DemoFiles() {
    PrintColor("DemoFiles");
    const Values<VectorOf<TX>> numbers{{5, 50}, {1, 10}, {4, 40}, {2, 20}, {7, 70}};
    TestFileRoundTrip<VectorOf<TX>>("Vector", numbers);
    TestFileRoundTrip<SortedLinkedList<TX>>("LinkedList", numbers);
    TestFileRoundTrip<SortedCircularLinkedList<TX>>("CircularLinkedList", numbers);
    TestFileRoundTrip<SortedDoubleLinkedList<TX>>("DoubleLinkedList", numbers);
    TestFileRoundTrip<SortedCircularDoubleLinkedList<TX>>("CircularDoubleLinkedList", numbers);
    TestFileRoundTrip<MinHeap<TX>>("Heap", numbers);
    TestFileRoundTrip<BinarySearchTree<TX>>("BinaryTree", numbers);
    TestFileRoundTrip<AVLSearchTree<TX>>("AVL", numbers);
    TestFileRoundTrip<BTreeOf<TX>>("BTree", numbers);
    TestFileRoundTrip<BPlusTreeOf<TX>>("BPlusTree", numbers);
    TestFileRoundTrip<VectorOf<std::string>>("VectorStrings", {{"hola mundo", 1}, {"a,(b)", 2}, {"[x]", 3}, {"", 4}});

    PrintColor("DemoFiles (archivo escrito a mano con identificadores)");
    const std::string path = "files_manual.txt";
    WriteText(path, "# id valor\n101 hola mundo\n\n102 segunda linea\r\n  103   con espacios\n");
    VectorOf<std::string> words;
    Expect(words.load_records(path) == 3, "Files: cantidad de registros");
    for (std::size_t i = 0; i < words.size(); ++i)
        std::cout << "  id " << words[i].getRef() << " -> \"" << words[i].getValue() << "\"\n";
    Expect(words[0].getRef() == 101 && words[0].getValue() == "hola mundo" && words[1].getValue() == "segunda linea" &&
               words[2].getRef() == 103 && words[2].getValue() == "con espacios",
           "Files: registros mal leidos");

    AVLSearchTree<TX> tree;
    WriteText(path, "30 3\n10 1\n20 2\n");
    tree.load_records(path);
    Expect(Sorted(tree) == std::vector<std::pair<TX, Ref>>{{1, 10}, {2, 20}, {3, 30}}, "Files: arbol desde registros");
    BPlusTreeOf<TX> index;
    index.load_records(path);
    Expect(index.find_ref(1) == std::optional<Ref>(10) && index.find_ref(2) == std::optional<Ref>(20),
           "Files: B+ desde registros");

    const auto before = Sorted(tree);
    for (const char* bad : {"30 3\nxx 5\n", "30\n", "30 abc\n", "30 3 extra\n40\n"}) {
        WriteText(path, bad);
        Bool rejected = false;
        try {
            tree.load_records(path);
        } catch (const std::runtime_error& error) {
            rejected = true;
            std::cout << "  rechazado: " << error.what() << '\n';
        }
        Expect(rejected && Sorted(tree) == before, std::string("Files: registro invalido mal manejado: ") + bad);
    }
    Bool rejected = false;
    try {
        tree.load_records("no_existe.rec");
    } catch (const std::runtime_error& error) {
        rejected = true;
        std::cout << "  rechazado: " << error.what() << '\n';
    }
    Expect(rejected, "Files: abrio un archivo inexistente");
    rejected = false;
    try {
        tree.load("no_existe.txt");
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Expect(rejected, "Files: load abrio un archivo inexistente");
    std::remove(path.c_str());
    std::cout << "Files: OK\n";
}
