
#include <iostream>
#include <fstream> // ofstream para escribir en archivo
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include "foreach.h"
#include "containers/vector.h"
#include "containers/Heap.h"
#include "containers/linkedlist.h"
#include "containers/circularlinkedlist.h"
#include "containers/doublylinkedlist.h"
#include "containers/circulardoublylinkedlist.h"
#include "containers/binarytree.h"
#include "Demos.h"
using namespace std;

const bool UseColors = isatty(STDOUT_FILENO) && getenv("NO_COLOR") == nullptr;

void PrintHeader(const string &color, const string &text) {
    cout << endl;
    if (UseColors)
        cout << color;
    cout << text;
    if (UseColors)
        cout << "\033[0m";
    cout << endl;
}

void PrintTitle(const string &title) {
    PrintHeader("\033[1;36m", "===== " + title + " =====");
}

void PrintSubtitle(const string &subtitle) {
    PrintHeader("\033[1;33m", "--- " + subtitle + " ---");
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

template <typename Container>
void TestRing(Container &container) {
    using Node = typename Container::Node;

    Node *first = nullptr;
    Node *last = nullptr;
    size_t count = 0;
    for (auto &node : container) {
        if (!first)
            first = &node;
        last = &node;
        ++count;
    }

    cout << "Nodos recorridos con iteradores (una vuelta): " << count << endl;
    cout << "tail->m_pNext == root: " << ((last && last->m_pNext == first) ? "si" : "no") << endl;
    cout << "Siguiendo m_pNext durante dos vueltas: [";
    Node *curr = first;
    for (size_t i = 0; curr && i < 2 * count; ++i) {
        cout << *curr << " ";
        curr = curr->m_pNext;
    }
    cout << "]" << endl;
}

template <typename Container>
void TestBackLinks(Container &container) {
    using Node = typename Container::Node;

    Node *first = nullptr;
    Node *last = nullptr;
    bool consistent = true;
    for (auto &node : container) {
        if (!first)
            first = &node;
        last = &node;
        if (node.m_pNext && node.m_pNext->m_pPrev != &node)
            consistent = false;
    }

    cout << "m_pPrev coherente con m_pNext en todos los nodos: " << (consistent ? "si" : "no") << endl;
    if (first)
        cout << "root->m_pPrev: " << (first->m_pPrev == nullptr ? "nullptr" : (first->m_pPrev == last ? "tail" : "otro")) << endl;
}

template <typename Container>
void TestBackwardIterator(Container &container) {
    cout << "Backward con rbegin()/rend(): [";
    for (auto it = container.rbegin(); it != container.rend(); ++it)
        cout << *it << " ";
    cout << "]" << endl;
}

template <typename Container>
void TestRead(Container &container, const string &label, const string &filename) {
    ifstream in(filename);
    in >> container;
    in.close();
    cout << label << " desde archivo: " << container << endl;
    TestTraversal(container);
}

template <typename Container, typename Checker>
void TestCopy(Container &container, const string &label, Checker check) {
    Container copy(container);
    check(copy);
    copy.push_back(99, 9);
    cout << label << " copia con un push_back extra: " << copy << endl;
    cout << label << " original sin cambios: " << container << endl;
    check(copy);

    Container assigned;
    assigned = copy;
    cout << label << " tras la asignacion: " << assigned << endl;
    check(assigned);
}

template <typename Container, typename Checker>
void TestListOperations(Container &list, const string &label, const string &filename, Checker check) {
    list.insert(4, 12);
    list.insert(11, 20);
    list.insert(7, 6);
    cout << label << " luego de hacer 3 inserts " << list << endl;
    check(list);

    list.ApplyFunction(AddOne);
    cout << label << " tras usar Add One a sus elementos: " << list << endl;
    list.ApplyFunction(AddX<TX>, TX(10));
    cout << label << " tras usar AddX(10) a sus elementos: " << list << endl;

    TestCopy(list, label, check);

    list.clear();
    cout << label << " luego de usar clear " << list << endl;
    list.push_back(10, 2);
    cout << label << " luego de un nuevo push_back " << list << endl;
    check(list);

    Container read_list;
    TestRead(read_list, label, filename);
    check(read_list);
}

template <typename Container>
void TestOrderedInsert(const string &label, const vector<pair<typename Container::value_type, Ref>> &values) {
    Container container;
    for (const auto &v : values)
        container.insert(v.first, v.second);
    cout << label << ": " << container << endl;
}

void DemoVector() {
    PrintTitle("Pruebas para Vector");
    // Dejamos los archivos vacios para que TestContainer acumule (append)
    // el estado del container tras cada paso
    ofstream("vector.txt", ios::trunc).close();

    PrintSubtitle("Vector<int>");
    // Cada elemento es una pareja (valor, ref) que se guarda en un Node;
    // el constructor initializer_list arma el Vector inicial de una vez
    Vector<VectorAscTraits<TX>> vec({{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});
    TestContainer(vec, {{5, 15}, {6, 16}, {7, 17}, {8, 18}, {9, 19}}, "vector.txt");
    TestTraversal(vec);

    PrintSubtitle("Vector<int> leido desde archivo");
    Vector<VectorAscTraits<TX>> readVec;
    TestRead(readVec, "Vector", "vector.txt");

    PrintSubtitle("Vector<string>");
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
template <typename Container>
void TestRaceCondition(Container &container, const string &label, size_t N) {
    using T = typename Container::value_type;

    vector<pair<T, Ref>> values(N);
    for (size_t i = 0; i < N; ++i)
        values[i] = {static_cast<T>(i), static_cast<Ref>(i)};

    // Insercion concurrente: NThreads workers insertando en paralelo sobre
    // el mismo Container, cada uno con un subconjunto entrelazado (stride)
    vector<thread> workers;
    for (int t = 0; t < NThreads; ++t) {
        workers.emplace_back([&container, &values, N, t](){
            for (size_t i = t; i < N; i += NThreads)
                container.push_back(values[i].first, values[i].second);
        });
    }
    for (auto &worker : workers)
        worker.join();

    long long expectedSum = 0;
    for (auto &v : values) expectedSum += v.first;

    long long actualSum = 0;
    for (auto &node : container) actualSum += node.getValue();

    cout << label << ": se esperaban " << N << " elementos, "
         << "el container quedo con " << container.size() << endl;
    cout << "  suma esperada = " << expectedSum
         << ", suma obtenida = " << actualSum << endl;

    if (container.size() != N || actualSum != expectedSum)
        cout << "  *** Race condition detectada: se perdieron inserciones (push_back / resize sin sincronizar) ***" << endl;
    else
        cout << "  No se perdio ningun elemento: el mutex de push_back/resize "
             << "sincroniza correctamente las inserciones concurrentes" << endl;
}

void DemoRaceCondition() {
    PrintTitle("Pruebas de concurrencia para Vector");
    Vector<VectorAscTraits<TX>> vec;
    TestRaceCondition(vec, "Vector", 200000);
}

template <typename Container>
void TestHeap(Container &heap, const string &label) {
    cout << label << " top(): " << heap.top() << endl;
    cout << label << " pop() hasta vaciar: [";
    while (!heap.empty())
        cout << heap.pop() << " ";
    cout << "]" << endl;
}

template <typename Container>
void TestPopOrder(Container &heap, const string &label) {
    using T = typename Container::value_type;
    typename Container::Compare comp;
    bool ordered = true;
    T prev{};
    for (size_t i = 0; !heap.empty(); ++i) {
        T value = heap.pop().getValue();
        if (i > 0 && comp(value, prev))
            ordered = false;
        prev = value;
    }
    cout << label << " pop() en orden: " << (ordered ? "si" : "no") << endl;
}

void DemoHeap()
{
    PrintTitle("Pruebas para Heap");
    ofstream("heap.txt", ios::trunc).close();
    ofstream("heap_str.txt", ios::trunc).close();

    const vector<pair<TX, Ref>> values = {{5, 15}, {1, 11}, {8, 18}, {3, 13}, {7, 17}, {3, 23}};

    PrintSubtitle("MinHeap<int>");
    MinHeap<TX> minHeap;
    TestContainer(minHeap, values, "heap.txt");
    TestTraversal(minHeap);
    TestHeap(minHeap, "MinHeap");

    PrintSubtitle("MaxHeap<int>");
    MaxHeap<TX> maxHeap;
    TestContainer(maxHeap, values, "heap.txt");
    TestHeap(maxHeap, "MaxHeap");

    PrintSubtitle("MaxHeap<string>");
    MaxHeap<string> strHeap;
    TestContainer(strHeap, {{"pera", 1}, {"manzana", 2}, {"uva", 3}, {"kiwi", 4}}, "heap_str.txt");
    TestHeap(strHeap, "MaxHeap<string>");

    PrintSubtitle("MinHeap<int> leido desde archivo");
    MinHeap<TX> readHeap;
    TestRead(readHeap, "MinHeap", "heap.txt");

    PrintSubtitle("ApplyFunction(Square) sobre un MinHeap");
    MinHeap<TX> squares;
    InsertElements(squares, {{-4, 1}, {-3, 2}, {1, 3}, {2, 4}});
    cout << "MinHeap antes de ApplyFunction(Square): " << squares << endl;
    squares.ApplyFunction(Square);
    cout << "MinHeap tras ApplyFunction(Square): " << squares << endl;
    TestHeap(squares, "MinHeap tras Square");

    PrintSubtitle("pop() sobre un heap vacio");
    MinHeap<TX> empty;
    try {
        empty.pop();
    } catch (const out_of_range &e) {
        cout << "pop() sobre un heap vacio lanza: " << e.what() << endl;
    }

    PrintSubtitle("Concurrencia: MinHeap y MaxHeap");
    MinHeap<TX> minRace;
    TestRaceCondition(minRace, "MinHeap", 100000);
    TestPopOrder(minRace, "MinHeap");

    MaxHeap<TX> maxRace;
    TestRaceCondition(maxRace, "MaxHeap", 100000);
    TestPopOrder(maxRace, "MaxHeap");
}

// TODO: Implementar DemoLinkedList() para probar la lista enlazada y sus iteradores.
void DemoLinkedList()
{
    using IntLinkedList = LinkedList<LinkedListAscTraits<TX>>;

    PrintTitle("Pruebas para LinkedList");
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

void DemoCircularLinkedList()
{
    using IntCircularList = CircularLinkedList<CircularLinkedListAscTraits<TX>>;

    PrintTitle("Pruebas para CircularLinkedList");
    ofstream("list_circular.txt", ios::trunc).close();

    IntCircularList list;
    TestContainer(list, {{5, 15}, {1, 11}, {8, 18}, {3, 13}}, "list_circular.txt");
    TestTraversal(list);
    TestRing(list);

    TestListOperations(list, "CircularLinkedList", "list_circular.txt", [](auto &l) { TestRing(l); });
}

void DemoDoublyLinkedList()
{
    using IntDoublyList = DoublyLinkedList<DoublyLinkedListAscTraits<TX>>;

    PrintTitle("Pruebas para DoublyLinkedList");
    ofstream("list_doubly.txt", ios::trunc).close();

    IntDoublyList list;
    TestContainer(list, {{5, 15}, {1, 11}, {8, 18}, {3, 13}}, "list_doubly.txt");
    TestTraversal(list);
    TestBackwardIterator(list);
    TestBackLinks(list);

    TestListOperations(list, "DoublyLinkedList", "list_doubly.txt", [](auto &l) {
        TestBackLinks(l);
        TestBackwardIterator(l);
    });
}

void DemoCircularDoublyLinkedList()
{
    using IntDoublyCircularList = CircularDoublyLinkedList<CircularDoublyLinkedListAscTraits<TX>>;

    PrintTitle("Pruebas para CircularDoublyLinkedList");
    ofstream("list_doubly_circular.txt", ios::trunc).close();

    IntDoublyCircularList list;
    TestContainer(list, {{5, 15}, {1, 11}, {8, 18}, {3, 13}}, "list_doubly_circular.txt");
    TestTraversal(list);
    TestBackwardIterator(list);
    TestRing(list);
    TestBackLinks(list);

    TestListOperations(list, "CircularDoublyLinkedList", "list_doubly_circular.txt", [](auto &l) {
        TestRing(l);
        TestBackLinks(l);
        TestBackwardIterator(l);
    });
}

void DemoTraits()
{
    const vector<pair<TX, Ref>> values = {{5, 15}, {1, 11}, {8, 18}, {3, 13}, {7, 17}, {3, 23}};

    PrintTitle("Pruebas de Traits: insert ordenado en las cuatro listas");
    cout << "Insertando " << values.size() << " valores (5, 1, 8, 3, 7, 3) solo con insert()" << endl;
    TestOrderedInsert<LinkedList<LinkedListAscTraits<TX>>>("LinkedList Asc", values);
    TestOrderedInsert<LinkedList<LinkedListDescTraits<TX>>>("LinkedList Desc", values);
    TestOrderedInsert<CircularLinkedList<CircularLinkedListAscTraits<TX>>>("CircularLinkedList Asc", values);
    TestOrderedInsert<CircularLinkedList<CircularLinkedListDescTraits<TX>>>("CircularLinkedList Desc", values);
    TestOrderedInsert<DoublyLinkedList<DoublyLinkedListAscTraits<TX>>>("DoublyLinkedList Asc", values);
    TestOrderedInsert<DoublyLinkedList<DoublyLinkedListDescTraits<TX>>>("DoublyLinkedList Desc", values);
    TestOrderedInsert<CircularDoublyLinkedList<CircularDoublyLinkedListAscTraits<TX>>>("CircularDoublyLinkedList Asc", values);
    TestOrderedInsert<CircularDoublyLinkedList<CircularDoublyLinkedListDescTraits<TX>>>("CircularDoublyLinkedList Desc", values);
}

template <typename Container>
string ToString(const Container &container) {
    ostringstream os;
    os << container;
    return os.str();
}

template <typename Container>
void TestSorted(Container &container, const string &label) {
    using Node = typename Container::Node;
    typename Container::Compare comp;
    Node *prev = nullptr;
    bool sorted = true;
    for (auto &node : container) {
        if (prev && comp(node.getValue(), prev->getValue()))
            sorted = false;
        prev = &node;
    }
    cout << label << " recorrido en orden: " << (sorted ? "si" : "no") << endl;
}

template <typename Container>
void TestMove(Container &container, const string &label) {
    auto first = container.begin();
    Container moved(std::move(container));
    cout << label << " destino del move constructor: " << moved << endl;
    cout << label << " origen tras el move: " << container << ", size() = " << container.size() << endl;
    cout << label << " mismos nodos, sin copiar: " << (moved.begin() == first ? "si" : "no") << endl;

    container = std::move(moved);
    cout << label << " devuelto con move assignment: " << container << endl;
    cout << label << " origen del move assignment: " << moved << ", size() = " << moved.size() << endl;
}

struct Tracked {
    static inline long alive = 0;
    TX value;
    Tracked(TX v = 0) : value(v) { ++alive; }
    Tracked(const Tracked &other) : value(other.value) { ++alive; }
    Tracked &operator=(const Tracked &other) = default;
    ~Tracked() { --alive; }
    friend bool operator<(const Tracked &a, const Tracked &b) { return a.value < b.value; }
};

template <typename Container>
void TestDestructor(const string &label, TX depth) {
    {
        Container tree;
        for (TX v : {5, 1, 8, 3, 7})
            tree.insert(v, v);
        cout << label << " con 5 nodos, valores vivos: " << Tracked::alive << endl;
        {
            Container copy(tree);
            cout << label << " mas una copia, valores vivos: " << Tracked::alive << endl;
        }
        cout << label << " tras destruir la copia, valores vivos: " << Tracked::alive << endl;
        Container moved(std::move(tree));
        cout << label << " tras el move, valores vivos: " << Tracked::alive << endl;
    }
    cout << label << " tras destruir el original y el movido, valores vivos: " << Tracked::alive << endl;

    {
        Container chain;
        for (TX v = 0; v < depth; ++v)
            chain.insert(v, v);
        cout << label << " degenerado de " << depth << " niveles, valores vivos: " << Tracked::alive << endl;
    }
    cout << label << " tras destruir el degenerado, valores vivos: " << Tracked::alive << endl;
}

void DemoBinaryTree()
{
    using IntTree = BinaryTree<BinaryTreeAscTraits<TX>>;

    PrintTitle("Pruebas para BinaryTree");
    ofstream("binarytree.txt", ios::trunc).close();

    const vector<pair<TX, Ref>> values = {{5, 15}, {1, 11}, {8, 18}, {3, 13}, {7, 17}, {3, 23}, {9, 19}};

    PrintSubtitle("BinaryTree<int> ascendente: operator<< en preorden e impresion con indentacion");
    IntTree tree;
    TestContainer(tree, values, "binarytree.txt");
    tree.print(cout);

    PrintSubtitle("ForwardIterator y BackwardIterator");
    TestTraversal(tree);
    TestBackwardIterator(tree);

    PrintSubtitle("operator>>: lectura desde archivo");
    IntTree readTree;
    TestRead(readTree, "BinaryTree", "binarytree.txt");
    cout << "Misma forma que el original: " << (ToString(readTree) == ToString(tree) ? "si" : "no") << endl;

    PrintSubtitle("Constructor copia y operator= copia");
    TestCopy(tree, "BinaryTree", [](auto &t) { TestSorted(t, "BinaryTree"); });

    PrintSubtitle("Move constructor y move assignment");
    TestMove(tree, "BinaryTree");

    PrintSubtitle("Destructor seguro");
    TestDestructor<BinaryTree<BinaryTreeAscTraits<Tracked>>>("BinaryTree", 2000);

    PrintSubtitle("ApplyFunction, call, rcall y FirstThat heredados de GeneralContainer");
    tree.ApplyFunction(AddX<TX>, TX(10));
    cout << "BinaryTree tras ApplyFunction(AddX, 10): " << tree << endl;
    IntTree squares;
    InsertElements(squares, {{-4, 1}, {-3, 2}, {1, 3}, {2, 4}});
    cout << "BinaryTree antes de ApplyFunction(Square): " << squares << endl;
    squares.ApplyFunction(Square);
    cout << "BinaryTree tras ApplyFunction(Square): " << squares << endl;
    squares.print(cout);
    TestTraversal(squares);

    PrintSubtitle("BinaryTree<int> descendente");
    BinaryTree<BinaryTreeDescTraits<TX>> descTree;
    TestContainer(descTree, values, "binarytree.txt");
    descTree.print(cout);
    TestSorted(descTree, "BinaryTree descendente");
    TestBackwardIterator(descTree);

    PrintSubtitle("Concurrencia");
    IntTree raceTree;
    TestRaceCondition(raceTree, "BinaryTree", 5000);
    TestSorted(raceTree, "BinaryTree");
}
