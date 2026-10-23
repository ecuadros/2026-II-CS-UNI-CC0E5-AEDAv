
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <random>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <unistd.h>
#include "containers/aliases.h"
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

template <typename Value> void PrintColor(const Value& value, const char* color = "\033[1;36m") {
    const bool useColor = isatty(fileno(stdout)) && std::getenv("NO_COLOR") == nullptr;
    if (useColor) cout << color;
    cout << value;
    if (useColor) cout << "\033[0m";
    cout << '\n';
}

template <typename Function> void WithDemoFile(const string& filename, Function function) {
    auto removeFile = [&filename] {
        if (std::remove(filename.c_str()) != 0 && errno != ENOENT)
            cerr << "No se pudo borrar " << filename << '\n';
    };
    try {
        function();
    } catch (...) {
        removeFile();
        throw;
    }
    removeFile();
}

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
    WithDemoFile(filename, [&] {
        PrintColor("TestContainer (" + filename + ")");
        InsertElements(container, values);

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

        cout << "Container using cout directly: ";
        PrintColor(container);
    });
}

template <typename Container> void TestTraversal(Container& container, const string& name) {
    PrintColor("TestTraversal (" + name + ")");
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
    WithDemoFile(filename, [&] {
        PrintColor("TestGenericContainer (" + filename + ")");
        cout << "Vacio: " << container.empty() << '\n';

        container.push_back(1, 101);
        container.push_back(3, 103);
        container.push_back(7, 107);
        cout << "Luego de push_back: ";
        PrintColor(container);

        if constexpr (requires(Container& c) { c.insert(5, 105); }) {
            container.insert(5, 105);
            container.insert(2, 102);
            container.insert(9, 109);
            cout << "Luego de insert: ";
            PrintColor(container);

            Container copia(container);
            Container asignada;
            asignada = container;
            copia.insert(4, 104);
            cout << "Original: ";
            PrintColor(container);
            cout << "Copia modificada: ";
            PrintColor(copia);
            cout << "Asignada: ";
            PrintColor(asignada);
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
        cout << "Luego de reutilizarla: ";
        PrintColor(reutilizada);
    });
}

template <typename Container> void TestInvalidRead(Container& container, const string& name) {
    ostringstream before;
    before << container;
    istringstream invalid("[(7,70),broken]");
    if (invalid >> container) throw runtime_error(name + ": lectura invalida aceptada");
    ostringstream after;
    after << container;
    if (before.str() != after.str())
        throw runtime_error(name + ": la lectura invalida cambio los datos");
}

template <typename Container> void TestCopyAndClear(Container& container, const string& name) {
    PrintColor("TestCopyAndClear (" + name + ")");
    container.clear();
    container.push_back(8, 80);
    container.clear();
    container.clear();
    if (!container.empty() || container.begin() != container.end())
        throw runtime_error(name + ": clear no dejo el contenedor vacio");

    container.push_back(3, 30);
    if constexpr (requires { container.insert(1, 10); }) {
        container.insert(1, 10);
        container.insert(5, 50);
    } else {
    }
    Container copy(container);
    container = container;
    if (copy.empty() || container.empty()) throw runtime_error(name + ": fallo la copia");

    TestInvalidRead(container, name);

    container.clear();
    container.push_back(9, 90);
    if (container.empty() || (*container.begin()).getValue() != 9)
        throw runtime_error(name + ": no se pudo reutilizar el contenedor");
    cout << name << ": clear, copia y lectura invalida OK\n";
}

template <typename Container> void TestStructure(Container& container, const string& name);

void DemoVector() {
    PrintColor("DemoVector");

    VectorAsc<TX> vec;
    TestStructure(vec, "Vector");

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
    cout << "Vector: recorrido const y vacio OK\n";
}

template <typename Container>
void TestRaceCondition(const string& name,
                       const vector<pair<typename Container::value_type, Ref>>& values) {
    PrintColor("TestRaceCondition (" + name + ")");
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

    Ref expectedSum = 0;
    for (auto& v : values)
        expectedSum += v.first;

    size_t actualCount = 0;
    Ref actualSum = 0;
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
    PrintColor("DemoRaceCondition (" + name + ")");
    const size_t N = 200000;
    vector<pair<TX, Ref>> values(N);
    for (size_t i = 0; i < N; ++i)
        values[i] = {static_cast<TX>(i), static_cast<Ref>(i)};

    cout << "DemoRaceCondition (" << NThreads << " hilos, push_back):\n";
    TestRaceCondition<Container>(name, values);
}

void DemoLinkedList() {
    PrintColor("DemoLinkedList");
    using List = LinkedAscList<TX>;
    List lista;
    TestStructure(lista, "LinkedList");

    lista.ApplyFunction(AddOne);
    cout << "Tras AddOne: ";
    PrintColor(lista);
    lista.ApplyFunction(AddX<TX>, TX(10));
    cout << "Tras AddX(10): ";
    PrintColor(lista);
    DemoRaceCondition<List>("LinkedList");
}

void DemoCircularLinkedList() {
    PrintColor("DemoCircularLinkedList");
    using AscList = CircularLinkedAscList<TX>;
    using DescList = CircularLinkedDescList<TX>;
    AscList lista;
    TestStructure(lista, "CircularLinkedList");
    cout << "Ciclo: ";
    PrintCircularTraversal(lista.begin(), lista.end());

    DescList descendente;
    descendente.insert(5, 105);
    descendente.insert(3, 103);
    descendente.insert(7, 107);
    cout << "Lista descendente: ";
    PrintColor(descendente);
    cout << "Ciclo descendente: ";
    PrintCircularTraversal(descendente.begin(), descendente.end());
    DemoRaceCondition<AscList>("CircularLinkedList");
}

void DemoDoubleLinkedList() {
    PrintColor("DemoDoubleLinkedList");
    using AscList = DoubleLinkedAscList<TX>;
    using DescList = DoubleLinkedDescList<TX>;
    AscList lista;
    TestStructure(lista, "DoubleLinkedList");

    DescList descendente;
    descendente.insert(5, 105);
    descendente.insert(3, 103);
    descendente.insert(7, 107);
    cout << "Lista descendente: ";
    PrintColor(descendente);
    DemoRaceCondition<AscList>("DoubleLinkedList");
}

void DemoCircularDoubleLinkedList() {
    PrintColor("DemoCircularDoubleLinkedList");
    using AscList = CircularDoubleLinkedAscList<TX>;
    using DescList = CircularDoubleLinkedDescList<TX>;
    AscList lista;
    TestStructure(lista, "CircularDoubleLinkedList");
    cout << "Ciclo hacia adelante: ";
    PrintCircularTraversal(lista.begin(), lista.end());
    cout << "Ciclo hacia atras: ";
    PrintCircularTraversal(lista.rbegin(), lista.rend());

    DescList descendente;
    descendente.insert(5, 105);
    descendente.insert(3, 103);
    descendente.insert(7, 107);
    cout << "Lista descendente: ";
    PrintColor(descendente);
    cout << "Ciclo descendente hacia adelante: ";
    PrintCircularTraversal(descendente.begin(), descendente.end());
    cout << "Ciclo descendente hacia atras: ";
    PrintCircularTraversal(descendente.rbegin(), descendente.rend());
    DemoRaceCondition<AscList>("CircularDoubleLinkedList");
}

template <typename HeapType> void TestHeapOperations(HeapType& heap, const string& name) {
    PrintColor("TestHeapOperations (" + name + ")");
    heap.clear();
    typename HeapType::Compare compare;
    vector<typename HeapType::value_type> expected;
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
    TestInvalidRead(heap, name);
    cout << name << ": extraccion, concurrencia y lectura invalida OK\n";
}

template <typename View>
void TestOrder(const View& view, const vector<Ref>& expected, const string& name) {
    cout << name << ": [";
    vector<Ref> actual;
    for (const auto& node : view) {
        cout << node << ' ';
        actual.push_back(node.getRef());
    }
    cout << "]\n";
    if (actual != expected) throw runtime_error(name + ": orden de recorrido incorrecto");
}

template <typename Tree> void TestTreeOperations(Tree& tree) {
    TestOrder(tree.preorder(), {50, 10, 40, 20, 51, 70}, "Preorder");
    TestOrder(tree.inorder(), {10, 20, 40, 51, 50, 70}, "Inorder");
    TestOrder(tree.postorder(), {20, 51, 40, 10, 70, 50}, "Postorder");

    Tree emptyTree;
    TestOrder(emptyTree.preorder(), {}, "Preorder vacio");
    TestOrder(emptyTree.inorder(), {}, "Inorder vacio");
    TestOrder(emptyTree.postorder(), {}, "Postorder vacio");

    Tree singleNode{{8, 80}};
    TestOrder(singleNode.preorder(), {80}, "Preorder unico");
    TestOrder(singleNode.inorder(), {80}, "Inorder unico");
    TestOrder(singleNode.postorder(), {80}, "Postorder unico");

    cout << "Estructura del arbol:\n";
    tree.printIndented(cout);
}

template <typename Container> void TestStructure(Container& container, const string& name) {
    PrintColor(name);
    const vector<pair<typename Container::value_type, Ref>> values{{5, 50}, {1, 10}, {4, 40},
                                                                   {2, 20}, {7, 70}, {5, 51}};

    TestContainer(container, values, name + ".txt");
    TestTraversal(container, name);

    if constexpr (requires {
                      container.top();
                      container.pop();
                  }) {
        cout << "Top: " << container.top() << '\n';
        cout << "Orden de extraccion: ";
        while (!container.empty())
            cout << container.pop() << ' ';
        cout << '\n';
        TestHeapOperations(container, name);
    }

    if constexpr (requires {
                      container.preorder();
                      container.postorder();
                  })
        TestTreeOperations(container);

    container.clear();
    TestGenericContainer(container, name + "_persist.txt");
    TestInvalidRead(container, name);

    if constexpr (requires(const Container& source, Container& target) {
                      Container(source);
                      target = source;
                  }) {
        Container copyAndClear;
        TestCopyAndClear(copyAndClear, name);
    }

    TestRaceCondition<Container>(name, values);
}

template <typename Container> void TestStructure(const string& name) {
    Container container;
    TestStructure(container, name);
}

void DemoHeap() {
    TestStructure<MinHeap<int>>("MinHeap");
    TestStructure<MaxHeap<int>>("MaxHeap");
}

void DemoBinaryTree() {
    TestStructure<BinarySearchTree<TX>>("BinaryTree");
}
