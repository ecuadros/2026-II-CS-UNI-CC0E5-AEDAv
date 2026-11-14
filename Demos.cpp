
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <string>
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
#include "containers/hashtable.h"
#include "containers/sha256.h"
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
    { m.insert(entry).second } -> std::convertible_to<bool>;
    { m.emplace(entry).second } -> std::convertible_to<bool>;
    { m.find(key) } -> std::same_as<typename M::iterator>;
    { cm.find(key) } -> std::same_as<typename M::const_iterator>;
    { m.erase(key) } -> std::convertible_to<std::size_t>;
    { m.erase(it) } -> std::same_as<typename M::iterator>;
    { cm.contains(key) } -> std::convertible_to<bool>;
    { cm.size() } -> std::convertible_to<std::size_t>;
    { it == m.end() } -> std::convertible_to<bool>;
    { ++it } -> std::same_as<typename M::iterator&>;
    { it++ } -> std::same_as<typename M::iterator>;
    { it->first } -> std::same_as<const typename M::key_type&>;
    { it->second } -> std::same_as<typename M::mapped_type&>;
    { (*it).second } -> std::same_as<typename M::mapped_type&>;
    { cm.begin()->second } -> std::same_as<const typename M::mapped_type&>;
    { cm.rbegin() == cm.rend() } -> std::convertible_to<bool>;
    requires std::convertible_to<typename M::iterator, typename M::const_iterator>;
};

/** Árbol de Merkle: raíz, pruebas de inclusión, verificación, `update` y `push_back`. */
template <typename M>
concept MerkleLike = requires(M& m, const M& cm, std::size_t index, const typename M::value_type& block,
                              const typename M::value_type* first, const typename M::Digest& root) {
    requires std::equality_comparable<typename M::Digest>;
    requires std::copy_constructible<M>;
    M(first, first);
    { cm.root_hash() } -> std::same_as<typename M::Digest>;
    { cm.proof(index) } -> std::same_as<typename M::Proof>;
    { M::verify(block, cm.proof(index), root) } -> std::same_as<bool>;
    m.update(index, block);
    m.push_back(block);
    m.clear();
    { cm.size() } -> std::convertible_to<std::size_t>;
    { cm.height() } -> std::convertible_to<std::size_t>;
    { cm.empty() } -> std::convertible_to<bool>;
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
const int NThreads = 5;

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
    for (int t = 0; t < NThreads; ++t) {
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
        bool removed = tree.remove(value);
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
    int balance(const Node* n) const {
        return static_cast<int>(this->height(n->m_pChild[1])) - static_cast<int>(this->height(n->m_pChild[0]));
    }

    /** Elige el reemplazo igual que el AVL, para que ambos árboles eliminen el mismo nodo. */
    NodePtr replacement(NodePtr node) const override {
        bool side = balance(node) >= 0;
        return extreme(node->m_pChild[side], !side);
    }

    /** Escribe la clave con su factor de balance; el nodo marcado va entre `<` y `>`. */
    void print_node(std::ostream& os, const Node& node) const override {
        bool marked = &node == m_mark;
        int bf = balance(&node);
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
        int bf = balance(x);
        bool right = bf > 0;
        const Node* c = x->m_pChild[right];
        bool twice = balance(c) * bf < 0;
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
    using Table = HashTable<std::string, int>;
    Table table;
    int age = 18;
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
    Demo<MinHeap<int>>("MinHeap");
    Demo<MaxHeap<int>>("MaxHeap");
}

void DemoBinaryTree() { Demo<BinarySearchTree<TX>, BinarySearchTree<TX, std::greater<TX>>>("BinaryTree"); }

void DemoAVL() {
    Demo<AVLSearchTree<TX>, AVLSearchTree<TX, std::greater<TX>>>("AVL");
    DemoRaceCondition<AVLSearchTree<TX>>("AVL");
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
void Check(bool ok, const std::string& name, const std::string& what) {
    if (!ok) throw std::runtime_error(name + ": " + what);
}

/** Igual que Check() con el nombre `MerkleTree`. */
void CheckMerkle(bool ok, const std::string& what) { Check(ok, "MerkleTree", what); }

/** Imprime el subárbol tumbado, con el bloque junto a cada hoja. */
void PrintMerkleNode(const MerkleStrings::Node* node, const std::vector<std::string>& blocks, int depth) {
    if (!node) return;
    PrintMerkleNode(node->m_pChild[1], blocks, depth + 1);
    std::cout << std::string(depth * 8, ' ') << (node->is_leaf() ? blocks[node->getRef()] + ":" : "")
              << ShortHash(node->getValue()) << '\n';
    PrintMerkleNode(node->m_pChild[0], blocks, depth + 1);
}

/** Imprime el árbol con su raíz, cantidad de hojas y altura. */
void ShowMerkle(const MerkleStrings& tree, const std::vector<std::string>& blocks) {
    PrintMerkleNode(tree.tree_view().root(), blocks, 0);
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
    bool threw = false;
    try { tree.update(blocks.size(), extra); } catch (const std::out_of_range&) { threw = true; }
    Check(threw, name, "update fuera de rango no lanzo");
    std::cout << "update coincide con reconstruir y es reversible\n";
}

/** Prueba el árbol vacío, la copia independiente, `clear` y el árbol de una sola hoja. */
template <MerkleLike Tree> void TestMerkleCopyAndClear(const Blocks<Tree>& blocks, const std::string& name) {
    PrintColor("TestCopyAndClear (" + name + ")");
    Tree tree;
    bool threw = false;
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

/** Llama a `update`, `proof` y `root_hash` desde varios hilos y comprueba que el estado final es consistente. */
template <MerkleLike Tree>
void TestMerkleRace(const Blocks<Tree>& blocks, const typename Tree::value_type& extra, const std::string& name) {
    PrintColor("TestRaceCondition (" + name + ")");
    Check(blocks.size() >= std::size_t(NThreads), name, "faltan bloques para la prueba de hilos");
    Tree tree = BuildMerkle<Tree>(blocks);
    std::vector<std::thread> threads;
    for (int t = 0; t < NThreads; ++t)
        threads.emplace_back([&, t] {
            for (int i = 0; i < 2000; ++i) {
                tree.update(t, i % 2 ? blocks[t] : extra);
                (void)tree.proof(t);
                (void)tree.root_hash();
            }
        });
    for (auto& thread : threads) thread.join();
    auto edited = blocks;
    for (int t = 0; t < NThreads; ++t) {
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
    for (char letter = 'A'; letter <= 'I'; ++letter) txids.push_back(Sha256::hash(std::string(1, letter)));
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
    int level = 0;
    for (auto&& step : tree.proof(target)) {
        const auto& s = step.getValue();
        hash = s.sibling_is_right ? MerkleHasher::combine(hash, s.sibling) : MerkleHasher::combine(s.sibling, hash);
        std::string prev = "h" + std::to_string(level++);
        std::cout << "h" << level << " = H("
                  << (s.sibling_is_right ? prev + " || " + ShortHash(s.sibling) : ShortHash(s.sibling) + " || " + prev)
                  << ") = " << ShortHash(hash) << '\n';
    }
    bool ok = hash == tree.root_hash();
    PrintColor("h" + std::to_string(level) + " == raiz (" + ShortHash(tree.root_hash()) + "): " +
                   (ok ? "verifica" : "NO verifica"),
               ok ? "1;32" : "1;31");
    CheckMerkle(ok, "la prueba paso a paso no verifico");
    CheckMerkle(!MerkleStrings::verify("D2", tree.proof(target), tree.root_hash()), "bloque alterado verifico");
    PrintColor("verify con el bloque alterado \"D2\": no verifica", "1;32");
}

/** Prueba la interfaz de mapa: acceso, búsqueda, inserción, borrado, recorrido, lectura y escritura. */
template <MapLike Map> void TestMap(const std::string& name) {
    auto check = [&](bool ok, const std::string& what) { Check(ok, name, what); };
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
    int suma = 0;
    for (const auto& [clave, edad] : lectura) suma += edad;
    for (auto it = lectura.rbegin(); it != lectura.rend(); ++it) suma -= it->second;
    check(suma == 0 && lectura.find("ana")->second == 22 && (*edades.begin()).first == "ana", "recorrido const fallo");

    typename Map::const_iterator fijo = edades.find("beto");
    check(fijo++->first == "beto" && fijo->first == "eva" && lectura.at("eva") == 32, "it++/const_iterator/at fallo");
    edades.at("eva") = 33;
    bool lanzo = false;
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
    TestInvalidRead(edades, name);
    std::cout << name << " leido de texto desordenado: " << edades << '\n';
    edades.print_tree(std::cout);
    std::cout << name << ": interfaz de mapa OK\n";
}

void DemoHashTable() {
    PrintColor("DemoHashTable (interfaz de mapa, clave string)");
    TestMap<HashTable<std::string, int>>("HashTable");
    TestTree<HashTable<TX, Ref>::Tree>("HashTable (AVL interno)");
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
