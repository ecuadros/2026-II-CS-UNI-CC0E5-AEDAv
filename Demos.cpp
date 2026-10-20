
#include <iostream>
#include <fstream> // ofstream para escribir en archivo
#include <string>
#include <thread>
#include <vector>
#include "foreach.h"
#include "containers/vector.h"
#include "containers/linkedlist.h"
#include "Demos.h"
#include "containers/Heap.h"
using namespace std;
#include <sstream>

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

    cout << "Prueba de función call con std::cout para LinkedList [";
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
    
    //demo
    Vector<VectorAscTraits<TX>> vecLeido;
    vecLeido.push_back(99, 999);

    ifstream archivo("vector.txt");
    archivo >> vecLeido;

    cout << "Vector leido desde archivo: " << vecLeido << '\n';

    std::istringstream entradaVacia("[]");
    entradaVacia >> vecLeido;

    cout << "Vector tras leer entrada vacia: " << vecLeido << '\n';

    //
    Vector<VectorAscTraits<string>> vecTextoLeido;

    ifstream archivoTexto("vector_str.txt");
    archivoTexto >> vecTextoLeido;

    cout << "Vector de cadenas leido: " << vecTextoLeido << '\n';
    cout << "Fallo de lectura: " << archivoTexto.fail() << '\n';
}

// Insertamos muchos elementos (generados en un loop, no a mano)
// concurrentemente y comparamos cuantos deberian haber entrado contra
// cuantos entraron realmente. Si push_back no estuviera sincronizado
// (sin el mutex/lock_guard actual) esto perderia inserciones o crashearia;
// con el mutex protegiendo push_back/resize, deberia dar siempre 0 perdidas.

template <typename Func>
void RunConcurrent(size_t n, Func func) {
    vector<thread> workers;

    for (int t = 0; t < NThreads; ++t) {
        workers.emplace_back([&func, n, t]() {
            for (size_t i = t; i < n; i += NThreads) {
                func(i);
            }
        });
    }

    for (auto& worker : workers) {
        worker.join();
    }
}

//
template <typename HeapType>
void TestConcurrentHeap(HeapType& heap, size_t n) {
    RunConcurrent(n, [&heap](size_t i) {
        heap.push(
            static_cast<TX>(i),
            static_cast<Ref>(i)
        );
    });

    typename HeapType::Compare comp;
    const bool ascendente = comp(TX{0}, TX{1});
    size_t errores = 0;

    for (size_t i = 0; i < n; ++i) {
        auto node = heap.pop();
        size_t esperado = ascendente ? i : n - 1 - i;

        if (node.getValue() != static_cast<TX>(esperado) ||
            node.getRef() != static_cast<Ref>(esperado)) {
            ++errores;
        }
    }

    cout << "Nodos incorrectos: " << errores << '\n';
}




void DemoRaceCondition() {
    const size_t N = 200000;

    vector<pair<TX, Ref>> values(N);
    for (size_t i = 0; i < N; ++i)
        values[i] = {static_cast<TX>(i), static_cast<Ref>(i)};

    Vector<VectorAscTraits<TX>> vec;

    // Insercion concurrente: NThreads workers insertando en paralelo sobre
    // el mismo Vector, cada uno con un subconjunto entrelazado (stride)
   RunConcurrent(values.size(), [&vec, &values](size_t i) {
    vec.push_back(values[i].first, values[i].second);
});

    long long expectedSum = 0;
    for (auto &v : values) expectedSum += v.first;

    long long actualSum = 0;
    for (size_t i = 0; i < vec.size(); ++i) actualSum += vec[i].getValue();

    cout << "DemoRaceCondition: se esperaban " << N << " elementos, "
         << "el Vector quedo con " << vec.size() << endl;
    cout << "  suma esperada = " << expectedSum
         << ", suma obtenida = " << actualSum << endl;

    if (vec.size() != N || actualSum != expectedSum)
        cout << "  *** Race condition detectada: se perdieron inserciones (push_back / resize sin sincronizar) ***" << endl;
    else
        cout << "  No se perdio ningun elemento: el mutex de push_back/resize "
             << "sincroniza correctamente las inserciones concurrentes" << endl;
}

// TODO: Implementar DemoLinkedList() para probar la lista enlazada y sus iteradores.
void DemoLinkedList()
{
    using IntLinkedList = LinkedList<LinkedListAscTraits<TX>>;

    ofstream("linkedlist.txt", ios::trunc).close();

    // Pruebas de pushback
    IntLinkedList list;
    TestContainer(list, {{5, 15}, {1, 11}, {8, 18}, {3, 13}}, "linkedlist.txt");
    TestForwardTraversal(list);

    auto elements = std::vector<pair<TX, Ref>>({{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});
    for(auto& e : elements)
        list.push_back(e.first, e.second);

    // Pruebas de insert
    list.insert(4, 12);
    list.insert(11, 20);
    list.insert(7, 6);
    cout << "LinkedList luego de hacer 3 inserts " << list << endl;

    // Prueba de apply function
    list.ApplyFunction(AddOne);
    cout << "LinkedList tras usar Add One a sus elementos: " << list << endl;
    list.ApplyFunction(AddX<TX>, TX(10));
    cout << "LinkedList tras usar AddX(10) a sus elementos: " << list << endl;

    // Prueba de clear
    list.clear();
    cout << "LinkedList luego de usar clear" << list << endl;
    list.push_back(10, 2);
    cout << "LinkedList luego de un nuevo push_back " << list << endl;

    // Prueba de lectura
    IntLinkedList new_list;
    ifstream in("linkedlist.txt");
    in >> new_list;
    in.close();
    cout << "LinkedList leida desde archivo: " << new_list << endl;
    TestForwardTraversal(new_list);

    //demo dos listas
       LinkedList<LinkedListAscTraits<TX>> asc;
    LinkedList<LinkedListDescTraits<TX>> desc;

    for (const auto& p : {
        pair<TX, Ref>{5, 15},
        pair<TX, Ref>{1, 11},
        pair<TX, Ref>{8, 18},
        pair<TX, Ref>{3, 13}
    }) {
        asc.insert(p.first, p.second);
        desc.insert(p.first, p.second);
    }

    cout << "Ascendente: " << asc << '\n';
    cout << "Descendente: " << desc << '\n';
}

//demo plantilla de heap
template <typename HeapType>
void TestHeap(HeapType& heap) {
    heap.push(5, 15);
    heap.push(1, 11);
    heap.push(8, 18);
    heap.push(3, 13);
    heap.push(4, 14);
    heap.push(6, 16);
    heap.push(7, 17);

    ifstream archivoHeap("linkedlist.txt");
    archivoHeap >> heap;
    // Aplicamos funciones a los elementos del heap
    heap.ApplyFunction(AddX<TX>, TX(-6));
    heap.ApplyFunction(Square);

    cout << "Heap antes de extraer: " << heap << '\n';

    cout << "Extracciones: ";
    for (int i = 0; i < 4; ++i)
        cout << heap.pop() << " ";

    cout << '\n';
    cout << "Heap despues de extraer: " << heap << '\n';
}

//demo de heap
void DemoHeap() {
    MinHeap<TX> minHeap;
    MaxHeap<TX> maxHeap;

    cout << "MinHeap: ";
    TestHeap(minHeap);

    cout << "MaxHeap: ";
    TestHeap(maxHeap);

    MinHeap<TX> minConcurrente;
    MaxHeap<TX> maxConcurrente;

    cout << "Prueba concurrente MinHeap: ";
    TestConcurrentHeap(minConcurrente, 20000);

    cout << "Prueba concurrente MaxHeap: ";
    TestConcurrentHeap(maxConcurrente, 20000);
}
