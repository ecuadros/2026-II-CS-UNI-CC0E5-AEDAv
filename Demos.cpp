
#include <iostream>
#include <fstream> // ofstream para escribir en archivo
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <random>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <unistd.h>
#include "containers/aliases.h"
#include "foreach.h"
#include "Demos.h"
using namespace std;

void AddOne(GeneralNode<TX>& node) {
    node.value() += 1;
}

template <typename T> void AddX(GeneralNode<T>& node, T x) {
    node.value() += x;
}

void Square(GeneralNode<TX>& node) {
    node.value() *= node.value();
}

template <typename Node> void PrintNode(const Node& node, ostream& os) {
    os << node << " ";
}
const int NThreads = 5;

class DemoStatus {
    string m_name;
    int m_exceptions = std::uncaught_exceptions();

    static void Separate() {
        static bool first = true;
        if (!first) cout << '\n';
        first = false;
    }

    static bool UseColor() { return isatty(fileno(stdout)) && std::getenv("NO_COLOR") == nullptr; }

    static void Print(const char* state, const char* color, const string& name) {
        if (UseColor()) cout << color;
        cout << '[' << state << "] " << name;
        if (UseColor()) cout << "\033[0m";
        cout << '\n';
    }

public:
    explicit DemoStatus(string name) : m_name(std::move(name)) {
        Separate();
        Print("CORRIENDO", "\033[1;36m", m_name);
    }

    ~DemoStatus() {
        if (std::uncaught_exceptions() > m_exceptions)
            Print("ERROR", "\033[1;31m", m_name);
        else
            Print("FINALIZADO", "\033[1;32m", m_name);
    }
};

class ScopedDemoFile {
    string m_filename;

public:
    explicit ScopedDemoFile(string filename) : m_filename(std::move(filename)) {}
    ScopedDemoFile(const ScopedDemoFile&) = delete;
    ScopedDemoFile& operator=(const ScopedDemoFile&) = delete;

    ~ScopedDemoFile() {
        if (std::remove(m_filename.c_str()) != 0 && errno != ENOENT)
            cerr << "No se pudo borrar " << m_filename << '\n';
    }
};

// Inserta secuencialmente cada pareja (valor, ref) de 'values' en el
// Container, una por una. La insercion concurrente queda aislada en
// DemoRaceCondition(), que es donde se estudia esa problematica.
template <typename Container>
void InsertElements(Container& container,
                    const vector<pair<typename Container::value_type, Ref>>& values) {
    for (const auto& v : values)
        container.push_back(v.first, v.second);
}

template <typename Container>
void TestContainer(Container& container,
                   const vector<pair<typename Container::value_type, Ref>>& values,
                   const string& filename) {
    DemoStatus status("TestContainer (" + filename + ")");
    InsertElements(container, values);
    ScopedDemoFile cleanup(filename);

    if constexpr (requires { container.write(cout); }) {
        cout << "Container using write(): ";
        container.write(cout);
        cout << endl;
    }

    {
        ofstream of(filename, ios::trunc);
        if (!of) throw runtime_error("No se pudo crear " + filename);
        of << container << endl;
        if (!of) throw runtime_error("No se pudo escribir " + filename);
    }

    // Escritura en pantalla usando cout directamente (operator<<)
    cout << "Container using cout directly: ";
    cout << container << endl;
}

template <typename Container> void TestTraversal(Container& container, const string& name) {
    DemoStatus status("TestTraversal (" + name + ")");
    using Node = typename Container::Node;

    cout << "Forward traversal:  [";
    container.call(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Forward iterators:  [";
    for (auto&& node : container)
        cout << node << " ";
    cout << "]" << endl;

    if constexpr (requires(Container& c) {
                      c.rbegin();
                      c.rend();
                  }) {
        cout << "Backward traversal: [";
        container.rcall(PrintNode<Node>, cout);
        cout << "]" << endl;

        cout << "Backward iterators: [";
        for (auto it = container.rbegin(); it != container.rend(); ++it)
            cout << *it << " ";
        cout << "]" << endl;
    }

    auto it = container.begin();
    if (it != container.end() && ++it != container.end()) {
        Ref target = (*it).getRef();
        const Node& found = container.FirstThat(
            [](const Node& node, Ref ref) { return node.getRef() == ref; }, target);
        cout << "FirstThat (ref " << target << "): " << found << endl;
    }
}

template <typename Iterator> void PrintCircularTraversal(Iterator begin, Iterator end) {
    if (begin == end) {
        cout << "[]\n";
        return;
    }

    cout << *begin;
    auto it = begin;
    for (++it; it != end; ++it)
        cout << " -> " << *it;
    cout << " -> (vuelve a " << *begin << ")\n";
}

template <typename Container>
void TestGenericContainer(Container& container, const string& filename = "container.txt") {
    DemoStatus status("TestGenericContainer (" + filename + ")");
    ScopedDemoFile cleanup(filename);
    cout << "Vacio: " << container.empty() << '\n';

    container.push_back(1, 101);
    container.push_back(3, 103);
    container.push_back(7, 107);
    cout << "Luego de push_back: " << container << '\n';

    if constexpr (requires(Container& c) { c.insert(5, 105); }) {
        container.insert(5, 105);
        container.insert(2, 102);
        container.insert(9, 109);
        cout << "Luego de insert: " << container << '\n';

        Container copia(container);
        Container asignada;
        asignada = container;
        copia.insert(4, 104);
        cout << "Original: " << container << '\n';
        cout << "Copia modificada: " << copia << '\n';
        cout << "Asignada: " << asignada << '\n';
    }

    {
        ofstream archivo(filename);
        if (!archivo) throw runtime_error("No se pudo crear " + filename);
        archivo << container;
    }
    cout << "Escrito en " << filename << '\n';

    Container leida;
    {
        ifstream archivo(filename);
        if (!archivo) throw runtime_error("No se pudo abrir " + filename);
        if (!(archivo >> leida)) throw runtime_error("No se pudo leer " + filename);
    }
    cout << "Leido desde " << filename << ": " << leida << '\n';

    Container reutilizada;
    reutilizada.push_back(1, 101);
    reutilizada.clear();
    cout << "Luego de clear, vacia: " << reutilizada.empty() << '\n';
    reutilizada.push_back(10, 110);
    cout << "Luego de reutilizarla: " << reutilizada << '\n';
}

template <typename List> void VerifyLinkedList(const string& name) {
    DemoStatus status("VerifyLinkedList (" + name + ")");
    List list;
    list.clear();
    list.push_back(8, 80);
    list.clear();
    list.clear();
    if (!list.empty() || list.begin() != list.end())
        throw runtime_error(name + ": clear no dejo la lista vacia");

    list.push_back(3, 30);
    list.insert(1, 10);
    list.insert(5, 50);
    List copy(list);
    list = list;
    if (copy.empty() || list.empty()) throw runtime_error(name + ": fallo la copia");

    ostringstream before;
    before << list;
    istringstream invalid("[(7,70),broken]");
    if (invalid >> list) throw runtime_error(name + ": se acepto una lectura invalida");
    ostringstream after;
    after << list;
    if (before.str() != after.str())
        throw runtime_error(name + ": la lectura invalida altero la lista");

    list.clear();
    list.push_back(9, 90);
    if (list.empty() || (*list.begin()).getValue() != 9)
        throw runtime_error(name + ": no se pudo reutilizar la lista");
    cout << name << ": clear, copia y lectura invalida OK\n";
}

void DemoVector() {
    DemoStatus status("DemoVector");
    using Vec = VectorAsc<TX>;
    static_assert(std::is_same_v<decltype(*std::declval<Vec&>().begin()), Vec::Node&>);
    static_assert(std::is_same_v<decltype(*std::declval<Vec&>().rbegin()), Vec::Node&>);
    static_assert(std::is_same_v<decltype(*std::declval<const Vec&>().begin()),
                                 const Vec::Node&>);
    static_assert(std::is_same_v<decltype(*std::declval<const Vec&>().rbegin()),
                                 const Vec::Node&>);
    // Cada elemento es una pareja (valor, ref) que se guarda en un Node;
    // el constructor initializer_list arma el Vector inicial de una vez
    VectorAsc<TX> vec({{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});
    TestContainer(vec, {{5, 15}, {6, 16}, {7, 17}, {8, 18}, {9, 19}}, "vector.txt");
    TestTraversal(vec, "Vector<TX>");

    VectorAsc<string> strVec;
    TestContainer(strVec, {{"Hello", 1}, {"World", 2}}, "vector_str.txt");
    TestTraversal(strVec, "Vector<string>");

    const auto& constVec = vec;
    vector<Ref> forwardRefs;
    vector<Ref> backwardRefs;
    for (auto it = constVec.begin(); it != constVec.end(); it++)
        forwardRefs.push_back(it->getRef());
    for (auto it = constVec.rbegin(); it != constVec.rend(); it++)
        backwardRefs.push_back(it->getRef());
    std::reverse(backwardRefs.begin(), backwardRefs.end());
    if (forwardRefs.size() != vec.size() || forwardRefs != backwardRefs)
        throw runtime_error("Vector const: recorridos inconsistentes");

    VectorAsc<TX> emptyVector;
    if (emptyVector.rbegin() != emptyVector.rend())
        throw runtime_error("Vector vacio: recorrido inverso incorrecto");
    const auto& constEmpty = emptyVector;
    if (constEmpty.begin() != constEmpty.end() || constEmpty.rbegin() != constEmpty.rend())
        throw runtime_error("Vector const vacio: recorrido incorrecto");
    istringstream invalidVector("[(7,70),oops]");
    if (invalidVector >> vec || vec.size() != 10)
        throw runtime_error("Vector: lectura invalida altero los datos");
    cout << "Vector: recorrido vacio y lectura invalida OK\n";
}

template <typename Container>
void TestRaceCondition(const string& name, const vector<pair<TX, Ref>>& values) {
    DemoStatus status("TestRaceCondition (" + name + ")");
    Container container;

    size_t n = values.size();
    vector<thread> workers;
    for (int t = 0; t < NThreads; ++t) {
        workers.emplace_back([&container, &values, n, t]() {
            for (size_t i = t; i < n; i += NThreads)
                container.push_back(values[i].first, values[i].second);
        });
    }
    for (auto& worker : workers)
        worker.join();

    long long expectedSum = 0;
    for (auto& v : values)
        expectedSum += v.first;

    size_t actualCount = 0;
    long long actualSum = 0;
    for (auto it = container.begin(); it != container.end(); ++it) {
        ++actualCount;
        actualSum += (*it).getValue();
    }

    cout << "DemoRaceCondition (" << name << "): se esperaban " << n
         << " elementos, el Container quedo con " << actualCount << endl;
    cout << "  suma esperada = " << expectedSum << ", suma obtenida = " << actualSum << endl;

    if (actualCount != n || actualSum != expectedSum)
        cout << "  *** Race condition detectada: se perdieron inserciones (push_back sin "
                "sincronizar) ***"
             << endl;
    else
        cout << "  No se perdio ningun elemento: el mutex de push_back "
             << "sincroniza correctamente las inserciones concurrentes" << endl;
}

template <typename Container> void DemoRaceCondition(const string& name) {
    DemoStatus status("DemoRaceCondition (" + name + ")");
    const size_t N = 200000;
    vector<pair<TX, Ref>> values(N);
    for (size_t i = 0; i < N; ++i)
        values[i] = {static_cast<TX>(i), static_cast<Ref>(i)};

    cout << "DemoRaceCondition (" << NThreads << " hilos, push_back):\n";
    TestRaceCondition<Container>(name, values);
}

void DemoLinkedList() {
    DemoStatus status("DemoLinkedList");
    using List = LinkedAscList<TX>;
    List lista;
    TestGenericContainer(lista);
    TestTraversal(lista, "LinkedList");

    lista.ApplyFunction(AddOne);
    cout << "Tras AddOne: " << lista << '\n';
    lista.ApplyFunction(AddX<TX>, TX(10));
    cout << "Tras AddX(10): " << lista << '\n';
    DemoRaceCondition<List>("LinkedList");
    VerifyLinkedList<List>("LinkedList");
}

void DemoCircularLinkedList() {
    DemoStatus status("DemoCircularLinkedList");
    using AscList = CircularLinkedAscList<TX>;
    using DescList = CircularLinkedDescList<TX>;
    AscList lista;
    TestGenericContainer(lista);
    cout << "Ciclo: ";
    PrintCircularTraversal(lista.begin(), lista.end());
    TestTraversal(lista, "CircularLinkedList");

    DescList descendente;
    descendente.insert(5, 105);
    descendente.insert(3, 103);
    descendente.insert(7, 107);
    cout << "Lista descendente: " << descendente << '\n';
    cout << "Ciclo descendente: ";
    PrintCircularTraversal(descendente.begin(), descendente.end());
    DemoRaceCondition<AscList>("CircularLinkedList");
    VerifyLinkedList<AscList>("CircularLinkedList");
}

void DemoDoubleLinkedList() {
    DemoStatus status("DemoDoubleLinkedList");
    using AscList = DoubleLinkedAscList<TX>;
    using DescList = DoubleLinkedDescList<TX>;
    AscList lista;
    TestGenericContainer(lista);
    TestTraversal(lista, "DoubleLinkedList");

    DescList descendente;
    descendente.insert(5, 105);
    descendente.insert(3, 103);
    descendente.insert(7, 107);
    cout << "Lista descendente: " << descendente << '\n';
    DemoRaceCondition<AscList>("DoubleLinkedList");
    VerifyLinkedList<AscList>("DoubleLinkedList");
}

void DemoCircularDoubleLinkedList() {
    DemoStatus status("DemoCircularDoubleLinkedList");
    using AscList = CircularDoubleLinkedAscList<TX>;
    using DescList = CircularDoubleLinkedDescList<TX>;
    AscList lista;
    TestGenericContainer(lista);
    cout << "Ciclo hacia adelante: ";
    PrintCircularTraversal(lista.begin(), lista.end());
    cout << "Ciclo hacia atras: ";
    PrintCircularTraversal(lista.rbegin(), lista.rend());
    TestTraversal(lista, "CircularDoubleLinkedList");

    DescList descendente;
    descendente.insert(5, 105);
    descendente.insert(3, 103);
    descendente.insert(7, 107);
    cout << "Lista descendente: " << descendente << '\n';
    cout << "Ciclo descendente hacia adelante: ";
    PrintCircularTraversal(descendente.begin(), descendente.end());
    cout << "Ciclo descendente hacia atras: ";
    PrintCircularTraversal(descendente.rbegin(), descendente.rend());
    DemoRaceCondition<AscList>("CircularDoubleLinkedList");
    VerifyLinkedList<AscList>("CircularDoubleLinkedList");
}

template <typename HeapType, typename Compare>
void VerifyHeap(const string& name, Compare compare) {
    DemoStatus status("VerifyHeap (" + name + ")");
    HeapType heap;
    vector<int> expected;
    mt19937 rng(2026);
    uniform_int_distribution<int> distribution(-500, 500);
    for (int i = 0; i < 1500; ++i) {
        int value = distribution(rng);
        expected.push_back(value);
        heap.push(value, i);
    }
    sort(expected.begin(), expected.end(), compare);
    for (int value : expected) {
        if (heap.top().getValue() != value || heap.pop().getValue() != value)
            throw runtime_error(name + ": orden de extraccion incorrecto");
    }

    vector<thread> workers;
    for (int t = 0; t < 4; ++t)
        workers.emplace_back([&heap, t] {
            for (int i = 0; i < 1000; ++i)
                heap.push(t * 1000 + i, i);
        });
    for (auto& worker : workers)
        worker.join();
    if (heap.size() != 4000) throw runtime_error(name + ": se perdieron inserciones concurrentes");
    int previous = heap.pop().getValue();
    while (!heap.empty()) {
        int current = heap.pop().getValue();
        if (compare(current, previous)) throw runtime_error(name + ": heapify down incorrecto");
        previous = current;
    }

    heap.push(42, 7);
    istringstream invalid("[(1,2),broken]");
    if (invalid >> heap || heap.top().getValue() != 42)
        throw runtime_error(name + ": lectura invalida altero el heap");
    cout << name << ": extraccion, concurrencia y lectura invalida OK\n";
}

void DemoHeap() {
    DemoStatus status("DemoHeap");
    using Min = MinHeap<int>;
    using Max = MaxHeap<int>;
    static_assert(std::is_same_v<decltype(*std::declval<Min::ForwardIterator&>()),
                                 const Min::Node&>);
    static_assert(std::is_same_v<decltype(*std::declval<Min::BackwardIterator&>()),
                                 const Min::Node&>);
    const vector<pair<int, Ref>> values{{5, 50}, {1, 10}, {4, 40}, {2, 20}};

    {
        DemoStatus minStatus("MinHeap");
        Min minHeap;
        TestContainer(minHeap, values, "min_heap.txt");
        TestTraversal(minHeap, "MinHeap");
        cout << "Top: " << minHeap.top() << '\n';
        cout << "Orden de extraccion: ";
        while (!minHeap.empty())
            cout << minHeap.pop() << ' ';
        cout << '\n';
        TestGenericContainer(minHeap, "min_heap_persist.txt");
        VerifyHeap<Min>("MinHeap", less<int>{});
    }

    {
        DemoStatus maxStatus("MaxHeap");
        Max maxHeap;
        TestContainer(maxHeap, values, "max_heap.txt");
        TestTraversal(maxHeap, "MaxHeap");
        cout << "Top: " << maxHeap.top() << '\n';
        cout << "Orden de extraccion: ";
        while (!maxHeap.empty())
            cout << maxHeap.pop() << ' ';
        cout << '\n';
        TestGenericContainer(maxHeap, "max_heap_persist.txt");
        VerifyHeap<Max>("MaxHeap", greater<int>{});
    }
}

void DemoBinaryTree() {
    DemoStatus status("DemoBinaryTree");
    using Tree = BinarySearchTree<TX>;
    const vector<pair<TX, Ref>> values{{5, 50}, {1, 10}, {4, 40}, {2, 20}, {7, 70}, {5, 51}};

    auto showOrder = [](const string& name, const auto& view) {
        cout << name << ": [";
        for (const auto& node : view)
            cout << node << ' ';
        cout << "]\n";
    };

    auto verifyOrder = [](const auto& view, const vector<Ref>& expected) {
        vector<Ref> actual;
        for (const auto& node : view)
            actual.push_back(node.getRef());
        if (actual != expected) throw runtime_error("Orden de recorrido incorrecto");
    };

    Tree tree;
    TestContainer(tree, values, "binary_tree.txt");
    TestTraversal(tree, "BinaryTree");
    showOrder("Preorder", tree.preorder());
    showOrder("Inorder", tree.inorder());
    showOrder("Postorder", tree.postorder());
    verifyOrder(tree.preorder(), {50, 10, 40, 20, 51, 70});
    verifyOrder(tree.inorder(), {10, 20, 40, 51, 50, 70});
    verifyOrder(tree.postorder(), {20, 51, 40, 10, 70, 50});

    Tree emptyTree;
    if (emptyTree.preorder().begin() != emptyTree.preorder().end() ||
        emptyTree.inorder().begin() != emptyTree.inorder().end() ||
        emptyTree.postorder().begin() != emptyTree.postorder().end())
        throw runtime_error("Recorrido de arbol vacio");

    Tree singleNode{{8, 80}};
    verifyOrder(singleNode.preorder(), {80});
    verifyOrder(singleNode.inorder(), {80});
    verifyOrder(singleNode.postorder(), {80});

    cout << "Estructura del arbol:\n";
    tree.printIndented(cout);

    Tree genericTree;
    TestGenericContainer(genericTree, "binary_tree_persist.txt");
    TestRaceCondition<Tree>("BinaryTree", values);
}
