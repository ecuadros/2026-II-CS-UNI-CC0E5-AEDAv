
#include <iostream>
#include <fstream> // ofstream para escribir en archivo
#include <string>
#include <thread>
#include <vector>
#include "foreach.h"
#include "containers/vector.h"
#include "containers/linkedlist.h"
#include "containers/circularlinkedlist.h"
#include "containers/doublylinkedlist.h"
#include "containers/circulardoublylinkedlist.h"
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
}


void DemoCircularLinkedList() {
    cout << "\n=== DEMO CIRCULAR LINKED LIST (LC) ===" << endl;
    using IntCircularList = CircularLinkedList<LinkedListAscTraits<TX>>;

    ofstream("circularlist.txt", ios::trunc).close();

    IntCircularList clist;
    // 1. Inserción con push_back
    clist.push_back(10, 1);
    clist.push_back(20, 2);
    clist.push_back(30, 3);
    cout << "LC push_back inicial: " << clist << endl;

    // 2. Inserción ordenada con insert
    clist.insert(25, 25);
    clist.insert(5, 5);
    clist.insert(40, 40);
    cout << "LC tras insert ordenado: " << clist << endl;

    // 3. Imprimir N vueltas (Rúbrica oficial)
    cout << "LC (2 vueltas completas con print_loops): ";
    clist.print_loops(cout, 2);
    cout << endl;

    // 4. Recorrido con call
    cout << "LC recorrido con call (1 vuelta): [";
    clist.call([](const auto& node) { cout << "(" << node.getValue() << "," << node.getRef() << ") "; });
    cout << "]" << endl;

    // 5. Aplicar funciones (ApplyFunction)
    clist.ApplyFunction(AddOne);
    cout << "LC tras AddOne: " << clist << endl;

    // 6. Persistencia (escritura en archivo)
    ofstream out("circularlist.txt");
    out << clist;
    out.close();

    // 7. Prueba de clear y push_back post-clear
    clist.clear();
    cout << "LC tras clear (empty = " << (clist.empty() ? "true" : "false") << "): " << clist << endl;
    clist.push_back(99, 9);
    cout << "LC nuevo push_back tras clear: " << clist << endl;

    // 8. Prueba de lectura desde archivo
    IntCircularList read_clist;
    ifstream in("circularlist.txt");
    in >> read_clist;
    in.close();
    cout << "LC leida desde archivo: " << read_clist << endl;
    cout << "LC leida (2 vueltas): ";
    read_clist.print_loops(cout, 2);
    cout << endl;
}

void DemoDoublyLinkedList() {
    cout << "\n=== DEMO DOUBLY LINKED LIST (LDE) ===" << endl;
    using IntDoublyList = DoublyLinkedList<DoublyLinkedListAscTraits<TX>>;

    ofstream("doublylist.txt", ios::trunc).close();

    IntDoublyList dlist;
    // 1. push_back
    dlist.push_back(10, 1);
    dlist.push_back(20, 2);
    dlist.push_back(30, 3);
    cout << "LDE push_back inicial: " << dlist << endl;

    // 2. insert ordenado
    dlist.insert(25, 25);
    dlist.insert(5, 5);
    dlist.insert(50, 50);
    cout << "LDE tras insert ordenado: " << dlist << endl;

    // 3. TestTraversal (prueba call, rcall hacia atrás en O(1), y FirstThat)
    TestTraversal(dlist);

    // 4. ApplyFunction
    dlist.ApplyFunction(AddOne);
    cout << "LDE tras AddOne: " << dlist << endl;

    // 5. Persistencia (escritura)
    ofstream out("doublylist.txt");
    out << dlist;
    out.close();

    // 6. clear y push_back post-clear
    dlist.clear();
    cout << "LDE tras clear (empty = " << (dlist.empty() ? "true" : "false") << "): " << dlist << endl;
    dlist.push_back(100, 10);
    cout << "LDE nuevo push_back tras clear: " << dlist << endl;

    // 7. Lectura desde archivo
    IntDoublyList read_dlist;
    ifstream in("doublylist.txt");
    in >> read_dlist;
    in.close();
    cout << "LDE leida desde archivo: " << read_dlist << endl;
    TestTraversal(read_dlist);
}

void DemoCircularDoublyLinkedList() {
    cout << "\n=== DEMO CIRCULAR DOUBLY LINKED LIST (LDEC) ===" << endl;
    using IntCDList = CircularDoublyLinkedList<DoublyLinkedListAscTraits<TX>>;

    ofstream("circulardoublylist.txt", ios::trunc).close();

    IntCDList cdlist;
    // 1. push_back
    cdlist.push_back(100, 10);
    cdlist.push_back(200, 20);
    cdlist.push_back(300, 30);
    cout << "LDEC push_back inicial (1 vuelta): " << cdlist << endl;

    // 2. insert ordenado
    cdlist.insert(250, 25);
    cdlist.insert(50, 5);
    cdlist.insert(400, 40);
    cout << "LDEC tras insert ordenado: " << cdlist << endl;

    // 3. Vueltas adelante y atrás con print_loops
    cout << "LDEC (2 vueltas adelante): ";
    cdlist.print_loops(cout, 2, true);
    cout << endl;

    cout << "LDEC (2 vueltas atras):    ";
    cdlist.print_loops(cout, 2, false);
    cout << endl;

    // 4. rcall con 2 vueltas
    cout << "LDEC rcall (2 vueltas atras): [";
    cdlist.rcall([](const auto& node) { cout << "(" << node.getValue() << "," << node.getRef() << ") "; }, 2);
    cout << "]" << endl;

    // 5. ApplyFunction
    cdlist.ApplyFunction(AddOne);
    cout << "LDEC tras AddOne (1 vuelta): " << cdlist << endl;

    // 6. Persistencia (escritura)
    ofstream out("circulardoublylist.txt");
    out << cdlist;
    out.close();

    // 7. clear y push_back post-clear
    cdlist.clear();
    cout << "LDEC tras clear (empty = " << (cdlist.empty() ? "true" : "false") << "): " << cdlist << endl;
    cdlist.push_back(999, 99);
    cout << "LDEC nuevo push_back tras clear: " << cdlist << endl;

    // 8. Lectura desde archivo
    IntCDList read_cdlist;
    ifstream in("circulardoublylist.txt");
    in >> read_cdlist;
    in.close();
    cout << "LDEC leida desde archivo (1 vuelta): " << read_cdlist << endl;
    cout << "LDEC leida (2 vueltas atras): ";
    read_cdlist.print_loops(cout, 2, false);
    cout << endl;
}

