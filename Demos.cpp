
#include <iostream>
#include <fstream> // ofstream para escribir en archivo
#include <string>
#include <thread>
#include <vector>
#include "foreach.h"
#include "containers/vector.h"
#include "containers/linkedlist.h"
#include "containers/doublelinkedlist.h"
#include "containers/circularlinkedlist.h"
#include "containers/circulardoublelinkedlist.h"
#include "containers/Heap.h"
#include "containers/binarytree.h"
#include "Demos.h"
using namespace std;

enum class Color { Rojo = 31, Verde = 32, Amarillo = 33, Azul = 34, Magenta = 35, Cian = 36 };

template <typename T>
void PrintColor(const T &value, Color color, ostream &os = cout) {
    os << "\033[" << static_cast<int>(color) << "m" << value << "\033[0m";
}

void PrintTitle(const string &title, Color color = Color::Cian) {
    cout << "\n\033[1;" << static_cast<int>(color) << "m=== " << title << " ===\033[0m" << endl;
}

void PrintResult(bool ok, const string &message) {
    PrintColor(ok ? "[OK] " : "[FALLO] ", ok ? Color::Verde : Color::Rojo);
    cout << message << endl;
}

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

    cout << "Prueba de función call con std::cout: [";
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

void DemoVector() {
    PrintTitle("Pruebas del contenedor Vector");
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

// Insertamos muchos elementos (generados en un loop, no a mano)
// concurrentemente y comparamos cuantos deberian haber entrado contra
// cuantos entraron realmente. Si push_back no estuviera sincronizado
// (sin el mutex/lock_guard actual) esto perderia inserciones o crashearia;
// con el mutex protegiendo push_back/resize, deberia dar siempre 0 perdidas.
void DemoRaceCondition() {
    const size_t N = 200000;

    vector<pair<TX, Ref>> values(N);
    for (size_t i = 0; i < N; ++i)
        values[i] = {static_cast<TX>(i), static_cast<Ref>(i)};

    Vector<VectorAscTraits<TX>> vec;

    // Insercion concurrente: NThreads workers insertando en paralelo sobre
    // el mismo Vector, cada uno con un subconjunto entrelazado (stride)
    size_t n = values.size();
    vector<thread> workers;
    for (int t = 0; t < NThreads; ++t) {
        workers.emplace_back([&vec, &values, n, t](){
            for (size_t i = t; i < n; i += NThreads)
                vec.push_back(values[i].first, values[i].second);
        });
    }
    for (auto &worker : workers)
        worker.join();

    long long expectedSum = 0;
    for (auto &v : values) expectedSum += v.first;

    long long actualSum = 0;
    for (size_t i = 0; i < vec.size(); ++i) actualSum += vec[i].getValue();

    PrintTitle("Race condition en Vector");
    cout << "DemoRaceCondition: se esperaban " << N << " elementos, "
         << "el Vector quedo con " << vec.size() << endl;
    cout << "  suma esperada = " << expectedSum
         << ", suma obtenida = " << actualSum << endl;

    if (vec.size() != N || actualSum != expectedSum)
        PrintResult(false, "Race condition detectada: se perdieron inserciones (push_back / resize sin sincronizar)");
    else
        PrintResult(true, "No se perdio ningun elemento: el mutex de push_back/resize "
                          "sincroniza correctamente las inserciones concurrentes");
}

template <typename Container>
void TestLinkedList(Container &list, const string &filename) {
    ofstream(filename, ios::trunc).close();

    // Pruebas de push_back
    TestContainer(list, {{5, 15}, {1, 11}, {8, 18}, {3, 13}}, filename);
    TestForwardTraversal(list);

    InsertElements(list, {{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});

    // Pruebas de insert
    list.insert(4, 12);
    list.insert(11, 20);
    list.insert(7, 6);
    cout << "Lista luego de hacer 3 inserts: " << list << endl;

    // Prueba de ApplyFunction
    list.ApplyFunction(AddOne);
    cout << "Lista tras usar AddOne a sus elementos: " << list << endl;
    list.ApplyFunction(AddX<TX>, TX(10));
    cout << "Lista tras usar AddX(10) a sus elementos: " << list << endl;

    // Prueba de clear
    list.clear();
    cout << "Lista luego de usar clear: " << list << endl;
    list.push_back(10, 2);
    cout << "Lista luego de un nuevo push_back: " << list << endl;

    // Prueba de lectura y asignacion (operator=)
    Container new_list;
    ifstream in(filename);
    in >> new_list;
    in.close();
    cout << "Lista leida desde archivo: " << new_list << endl;
    TestForwardTraversal(new_list);

    list = new_list;
}

template <typename DoubleContainer>
void TestDoubleLinkedListSpecifics(DoubleContainer &list) {
    TestTraversal(list);

    list.clear();
    list.insert(5, 50);
    list.insert(1, 10);
    list.insert(9, 90);
    list.insert(6, 60);
    cout << "Lista tras inserts en cabeza, cola y medio: " << list << endl;
    TestTraversal(list);
}

void DemoLinkedList()
{
    PrintTitle("Pruebas para LinkedList", Color::Azul);
    LinkedList<LinkedListAscTraits<TX>> list;
    TestLinkedList(list, "linkedlist.txt");

    PrintTitle("Pruebas para DoubleLinkedList", Color::Azul);
    DoubleLinkedList<DoubleLinkedListAscTraits<TX>> dlist;
    TestLinkedList(dlist, "doublelinkedlist.txt");
    TestDoubleLinkedListSpecifics(dlist);

    PrintTitle("Pruebas para CircularLinkedList", Color::Azul);
    CircularLinkedList<LinkedListAscTraits<TX>> clist;
    TestLinkedList(clist, "circularlinkedlist.txt");

    PrintTitle("Pruebas para CircularDoubleLinkedList", Color::Azul);
    CircularDoubleLinkedList<DoubleLinkedListAscTraits<TX>> cdlist;
    TestLinkedList(cdlist, "circulardoublelinkedlist.txt");
    TestDoubleLinkedListSpecifics(cdlist);
}

template <typename Heap>
void PopAll(Heap &heap) {
    cout << "Pop hasta vaciar: ";
    while (!heap.empty())
        cout << heap.pop() << " ";
    cout << endl;
}

void DemoHeap() {
    PrintTitle("Pruebas para MaxHeap", Color::Magenta);
    ofstream("maxheap.txt", ios::trunc).close();
    MaxHeap<TX> maxHeap;
    TestContainer(maxHeap, {{5, 50}, {3, 30}, {8, 80}, {1, 10}, {9, 90}}, "maxheap.txt");
    cout << "top = " << maxHeap.top() << endl;
    maxHeap.ApplyFunction(AddX<TX>, 100);   // sumar lo mismo a todos conserva el orden
    cout << "Tras ApplyFunction(AddX, 100): " << maxHeap << endl;
    PopAll(maxHeap);

    PrintTitle("Pruebas para MinHeap", Color::Magenta);
    ofstream("minheap.txt", ios::trunc).close();
    MinHeap<TX> minHeap;
    TestContainer(minHeap, {{5, 50}, {3, 30}, {8, 80}, {1, 10}, {9, 90}, {2, 20}, {7, 70}}, "minheap.txt");
    TestForwardTraversal(minHeap);          // call, iteradores y FirstThat heredados 

    MinHeap<TX> leido;
    ifstream in("minheap.txt");
    in >> leido;
    cout << "MinHeap leido desde archivo: " << leido << endl;
    PopAll(leido);

    PrintTitle("MinHeap concurrente", Color::Magenta);
    const size_t N = 100000;
    MinHeap<TX> heap;
    vector<thread> workers;
    for (int t = 0; t < NThreads; ++t)
        workers.emplace_back([&heap, N, t]() {
            for (size_t i = t; i < N; i += NThreads)
                heap.push_back(static_cast<TX>(i), static_cast<Ref>(i));
        });
    for (auto &worker : workers)
        worker.join();


    size_t count = heap.size();
    bool ordered = true;
    TX prev = heap.pop().getValue();
    while (!heap.empty()) {
        TX v = heap.pop().getValue();
        if (v < prev) ordered = false;
        prev = v;
    }
    PrintResult(count == N && ordered,
                "Concurrencia: se esperaban " + to_string(N) + " elementos, el Heap quedo con " +
                to_string(count) + (ordered ? " y salen ordenados" : " y NO salen ordenados"));
}

void DemoBinaryTree() {
    PrintTitle("Pruebas para BinaryTree", Color::Verde);
    ofstream("binarytree.txt", ios::trunc).close();
    AscBinaryTree<TX> tree;
    TestContainer(tree, {{50, 500}, {30, 300}, {70, 700}, {20, 200}, {40, 400},
                         {60, 600}, {80, 800}, {35, 350}, {45, 450}, {65, 650}}, "binarytree.txt");
    TestTraversal(tree);              

    PrintTitle("BinaryTree (Desc)", Color::Verde);
    DescBinaryTree<TX> desc;
    InsertElements(desc, {{50, 500}, {30, 300}, {70, 700}, {20, 200}, {40, 400}});
    cout << "Desc: " << desc << endl;

    PrintTitle("BinaryTree: lectura desde archivo", Color::Verde);
    AscBinaryTree<TX> leido;
    ifstream in("binarytree.txt");
    in >> leido;
    cout << "El arbol binario: " << leido << endl;
    
    PrintTitle("BinaryTree concurrente", Color::Verde);
    const size_t N = 100000;
    AscBinaryTree<TX> concurrente;
    vector<thread> workers;
    for (int t = 0; t < NThreads; ++t)
        workers.emplace_back([&concurrente, N, t]() {
            for (size_t i = t; i < N; i += NThreads)
                concurrente.push_back((i * 7919) % N, i);
        });
    for (auto &worker : workers)
        worker.join();

    size_t count = 0;
    bool ordered = true;
    TX prev = -1;
    for (auto it = concurrente.begin(); it != concurrente.end(); ++it) {
        if ((*it).getValue() < prev) ordered = false;
        prev = (*it).getValue();
        ++count;
    }
    PrintResult(count == N && ordered,
                "Concurrencia: se esperaban " + to_string(N) + " elementos, el BinaryTree quedo con " +
                to_string(count) + (ordered ? " y en inorden salen ordenados" : " y NO salen ordenados"));
}
