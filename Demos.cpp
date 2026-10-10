
#include <iostream>
#include <fstream> // ofstream para escribir en archivo
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include "containers/circulardoublelinkedlist.h"
#include "containers/circularlinkedlist.h"
#include "containers/doublelinkedlist.h"
#include "containers/traits.h"
#include "foreach.h"
#include "containers/vector.h"
#include "containers/linkedlist.h"
#include "Demos.h"
using namespace std;

void AddOne(GeneralNode<TX> &node) {
    node.value() += 1;
}

template <typename T>
void AddX(GeneralNode<T> &node, T x) {
    node.value() += x;
}

void Square(GeneralNode<TX> &node) {
    node.value() *= node.value();
}

// Predicado generico para FirstThat/call: true si el valor del nodo es
// mayor que 'threshold'. Generico en Node y T para servir tanto a
// Vector<VectorAscTraits<TX>> (int) como a Vector<VectorAscTraits<string>>.
template <typename Node, typename T>
bool IsGreaterThan(Node &node, T threshold) {
    return node.getValue() > threshold;
}

template <typename Node>
void PrintNode(Node &node, ostream &os) {
    os << node << " ";
}
const int NThreads = 5;

// Inserta secuencialmente cada pareja (valor, ref) de 'values' en el
// Container, una por una. La insercion concurrente queda aislada en
// DemoRaceCondition(), que es donde se estudia esa problematica.
template <typename Container>
void InsertElements(Container &container,
                     const vector<pair<typename Container::value_type, Ref>> &values) {
    for (const auto &v : values)
        container.push_back(v.first, v.second);
}

template <typename Container>
void TestContainer(Container &container,
                    const vector<pair<typename Container::value_type, Ref>> &values,
                    const string &filename) {
    InsertElements(container, values);

    // Impresion usando write()
    cout << "Container using write(): ";
    container.write(cout);
    cout << endl;

    // Escritura hacia un archivo, en modo append para acumular cada estado
    // (el archivo se deja vacio una vez al inicio, ver DemoVector)
    ofstream of(filename, ios::app);
    container.write(of);
    of << endl;
    of.close();

    // Escritura en pantalla usando cout directamente (operator<<)
    cout << "Container using cout directly: ";
    cout << container << endl;
}

// Prueba los recorridos del Container hacia adelante (begin/end) y hacia
// atras (rbegin/rend) solamente, imprimiendo los elementos en cada sentido.
template <typename Container>
void TestTraversal(Container &container) {
    using Node = typename Container::Node;

    cout << "Forward traversal:  [";
    container.call(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Backward traversal: [";
    container.rcall(PrintNode<Node>, cout);
    cout << "]" << endl;

    // Prueba de call()/FirstThat() con valor de retorno: busca el primer
    // nodo cuyo valor sea mayor que el del primer elemento del container.
    if (!container.empty()) {
        auto threshold = (*container.begin()).getValue();
        Node &found = container.FirstThat(IsGreaterThan<Node, decltype(threshold)>, threshold);
        cout << "FirstThat (primer valor > " << threshold << "): " << found << endl;
    }
}

// Prueba los recorridos hacia únicamente hacia adelante, especialmente para una LinkedList
template <typename Container>
void TestForwardTraversal(Container &container){
    using Node = typename Container::Node;

    cout << "Prueba de función call con std::cout para List [";
    container.call(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Prueba de uso de iteradores para impresión: [";
    for (auto &&node : container)
        cout << node << " ";
    cout << "]" << endl;

    // Prueba de FirstThat(): primer nodo cuyo valor sea mayor que el primero
    if (container.begin() != container.end()) {
        auto threshold = (*container.begin()).getValue();
        Node &found = container.FirstThat(IsGreaterThan<Node, decltype(threshold)>, threshold);
        cout << "FirstThat (primer valor > " << threshold << "): " << found << endl;
    }
}

template <typename Container>
void TestBackwardTraversal(Container &container){
    using Node = typename Container::Node;

    cout << "Prueba de función rcall con std::cout para List [";
    container.rcall(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Prueba de uso de iteradores inversos para impresión: [";
    for (auto it = container.rbegin(); it != container.rend(); ++it)
        cout << *it << " ";
    cout << "]" << endl;
}

template <typename Iterator>
void PrintCircularTraversal(Iterator begin, Iterator end) {
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
void TestGenericContainer(Container &container) {
    cout << "Vacio: " << container.empty() << '\n';

    container.push_back(1, 101);
    container.push_back(3, 103);
    container.push_back(7, 107);
    cout << "Luego de push_back: " << container << '\n';

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

    const char *filename = "container.txt";
    {
        ofstream archivo(filename);
        if (!archivo) {
            cerr << "No se pudo crear " << filename << '\n';
            return;
        }
        archivo << '[';
        bool primero = true;
        for (const auto &nodo : container) {
            if (!primero)
                archivo << ',';
            archivo << nodo;
            primero = false;
        }
        archivo << ']';
    }
    cout << "Escrito en " << filename << '\n';

    Container leida;
    {
        ifstream archivo(filename);
        if (!archivo) {
            cerr << "No se pudo abrir " << filename << '\n';
            std::remove(filename);
            return;
        }
        leida.read(archivo);
    }
    cout << "Leido desde " << filename << ": " << leida << '\n';
    if (std::remove(filename) != 0)
        cerr << "No se pudo borrar " << filename << '\n';

    asignada.clear();
    cout << "Luego de clear, vacia: " << asignada.empty() << '\n';
    asignada.push_back(10, 110);
    cout << "Luego de reutilizarla: " << asignada << '\n';
}

void DemoVector() {
    // Dejamos los archivos vacios para que TestContainer acumule (append)
    // el estado del container tras cada paso
    ofstream("vector.txt", ios::trunc).close();

    // Cada elemento es una pareja (valor, ref) que se guarda en un Node;
    // el constructor initializer_list arma el Vector inicial de una vez
    Vector<VectorAscTraits<TX>> vec({{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});
    TestContainer(vec, {{5, 15}, {6, 16}, {7, 17}, {8, 18}, {9, 19}}, "vector.txt");
    TestTraversal(vec);

    ofstream("vector_str.txt", ios::trunc).close();
    Vector<VectorAscTraits<string>> strVec;
    TestContainer(strVec, {{"Hello", 1}, {"World", 2}}, "vector_str.txt");
    TestTraversal(strVec);
}

template <typename Container>
void TestRaceCondition(const string &name,
                       const vector<pair<TX, Ref>> &values) {
    Container container;

    size_t n = values.size();
    vector<thread> workers;
    for (int t = 0; t < NThreads; ++t) {
        workers.emplace_back([&container, &values, n, t](){
            for (size_t i = t; i < n; i += NThreads)
                container.push_back(values[i].first, values[i].second);
        });
    }
    for (auto &worker : workers)
        worker.join();

    long long expectedSum = 0;
    for (auto &v : values) expectedSum += v.first;

    size_t actualCount = 0;
    long long actualSum = 0;
    for (auto it = container.begin(); it != container.end(); ++it) {
        ++actualCount;
        actualSum += (*it).getValue();
    }

    cout << "DemoRaceCondition (" << name << "): se esperaban " << n
         << " elementos, el Container quedo con " << actualCount << endl;
    cout << "  suma esperada = " << expectedSum
         << ", suma obtenida = " << actualSum << endl;

    if (actualCount != n || actualSum != expectedSum)
        cout << "  *** Race condition detectada: se perdieron inserciones (push_back sin sincronizar) ***" << endl;
    else
        cout << "  No se perdio ningun elemento: el mutex de push_back "
             << "sincroniza correctamente las inserciones concurrentes" << endl;
}

template <typename Container>
void DemoRaceCondition(const string &name) {
    const size_t N = 200000;
    vector<pair<TX, Ref>> values(N);
    for (size_t i = 0; i < N; ++i)
        values[i] = {static_cast<TX>(i), static_cast<Ref>(i)};

    cout << "DemoRaceCondition (" << NThreads << " hilos, push_back):\n";
    TestRaceCondition<Container>(name, values);
}

void DemoLinkedList() {
    using List = LinkedList<LinkedListAscTraits<TX>>;
    cout << "DemoLinkedList:\n";
    List lista;
    TestGenericContainer(lista);
    TestForwardTraversal(lista);

    lista.ApplyFunction(AddOne);
    cout << "Tras AddOne: " << lista << '\n';
    lista.ApplyFunction(AddX<TX>, TX(10));
    cout << "Tras AddX(10): " << lista << '\n';
    DemoRaceCondition<List>("LinkedList");
}

void DemoCircularLinkedList() {
    using AscList = CircularLinkedList<LinkedListAscTraits<TX>>;
    using DescList = CircularLinkedList<LinkedListDescTraits<TX>>;
    cout << "DemoCircularLinkedList:\n";
    AscList lista;
    TestGenericContainer(lista);
    cout << "Ciclo: ";
    PrintCircularTraversal(lista.begin(), lista.end());
    TestForwardTraversal(lista);

    DescList descendente;
    descendente.insert(5, 105);
    descendente.insert(3, 103);
    descendente.insert(7, 107);
    cout << "Lista descendente: " << descendente << '\n';
    cout << "Ciclo descendente: ";
    PrintCircularTraversal(descendente.begin(), descendente.end());
    DemoRaceCondition<AscList>("CircularLinkedList");
}

void DemoDoubleLinkedList() {
    using AscList = DoubleLinkedList<DoubleLinkedListAscTraits<TX>>;
    using DescList = DoubleLinkedList<DoubleLinkedListDescTraits<TX>>;
    cout << "DemoDoubleLinkedList:\n";
    AscList lista;
    TestGenericContainer(lista);
    TestForwardTraversal(lista);
    TestBackwardTraversal(lista);

    DescList descendente;
    descendente.insert(5, 105);
    descendente.insert(3, 103);
    descendente.insert(7, 107);
    cout << "Lista descendente: " << descendente << '\n';
    DemoRaceCondition<AscList>("DoubleLinkedList");
}

void DemoCircularDoubleLinkedList() {
    using AscList = CircularDoubleLinkedList<DoubleLinkedListAscTraits<TX>>;
    using DescList = CircularDoubleLinkedList<DoubleLinkedListDescTraits<TX>>;
    cout << "DemoCircularDoubleLinkedList:\n";
    AscList lista;
    TestGenericContainer(lista);
    cout << "Ciclo hacia adelante: ";
    PrintCircularTraversal(lista.begin(), lista.end());
    cout << "Ciclo hacia atras: ";
    PrintCircularTraversal(lista.rbegin(), lista.rend());
    TestForwardTraversal(lista);
    TestBackwardTraversal(lista);

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
}
