/**
 * @file demos.cpp
 * @ingroup demos
 * @brief Implementación de las funciones de demostración declaradas en `demos.h`.
 */

#include <filesystem>
#include <iostream>
#include <ostream>
#include <fstream> // ofstream para escribir en archivo
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include "containers/vector.h"
#include "containers/lists/linked_list.h"
#include "containers/lists/double_linked_list.h"
#include "containers/lists/circular_linked_list.h"
#include "containers/lists/circular_double_linked_list.h"
#include "containers/trees/heap.h"
#include "containers/trees/binary_tree.h"
#include "containers/trees/avl_tree.h"
#include "containers/hash_table.h"
#include "containers/trees/merkle_tree.h"
#include "utilities/print_colored.h"
#include "utilities/gotoxy.h"
#include "utilities/render.h"
#include "utilities/layouts.h"
#include "demos.h"
#include <algorithm>
#include <iomanip>
#include <memory>
#include <numeric>
#include <random>
#include <stdexcept>
#include <string_view>
#include "containers/trees/btree.h"
#include "containers/trees/bstar_tree.h"
#include "utilities/btree_metrics.h"
#include "utilities/side_by_side.h"
#include "containers/trees/btree_validator.h"
using namespace std;

filesystem::path OUTPUT_DIR_PATH = "output";

void init_directories() {
    try {
        if (filesystem::create_directories(OUTPUT_DIR_PATH)) {
            cout << "Carpeta" << OUTPUT_DIR_PATH << " creada exitosamente.\n";
        }
    } catch (const filesystem::filesystem_error& e) {
        cout << "Error del sistema de archivos: " << e.what() << endl;
    }
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

template <typename T>
void ModX(GeneralNode<T> &node, T x) {
    node.value() %= x;
}

// Predicado generico para FirstThat/call: true si el valor del nodo es
// mayor que 'threshold'. Generico en Node y T para servir tanto a
// Vector<VectorAscTraits<TX>> (int) como a Vector<VectorAscTraits<string>>.
template <typename Node, typename T>
bool IsGreaterThan(Node &node, T threshold) {
    return node.getValue() > threshold;
}

template <typename Node, typename T>
bool IsEqualTo(Node &node, T target) {
    return node.getValue() == target;
}

template <typename Node>
void PrintNode(Node &node, ostream &os) {
    os << node << " ";
}
const TX NThreads = 5;

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
    init_directories();
    InsertElements(container, values);

    // Impresion usando operador de inserción
    cout << "Container using operator<<: ";
    cout << container;
    cout << endl;

    // Escritura hacia un archivo, en modo append para acumular cada estado
    // (el archivo se deja vacio una vez al inicio, ver DemoVector)
    std::filesystem::path file_path = OUTPUT_DIR_PATH / filename;
    ofstream of(file_path, ios::app);
    of << container;
    of << endl;
    of.close();

    // Escritura en pantalla usando cout directamente (operator<<)
    cout << "Container using cout directly: ";
    cout << container << endl;
}

// Prueba los recorridos del Container hacia adelante (begin/end) y hacia
// atras (rbegin/rend) solamente, imprimiendo los elementos en cada sentido.
template <typename Container>
void TestTraversal(Container &container, const typename Container::value_type &threshold) {
    using Node      = typename Container::Node;
    using ValueType = typename Container::value_type;

    cout << "Forward traversal:  [";
    container.call(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Backward traversal: [";
    container.rcall(PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Vista invertida (operator<<): " << container.reversed() << endl;

    Node &found = container.FirstThat(IsGreaterThan<Node, ValueType>, threshold);
    cout << "FirstThat (primer valor > " << threshold << "): " << found << endl;

    Node &rev_found = container.reversed().FirstThat(IsGreaterThan<Node, ValueType>, threshold);
    cout << "reversed().FirstThat (ultimo valor > " << threshold << "): " << rev_found << endl;
}

// Prueba los recorridos hacia únicamente hacia adelante, especialmente para una LinkedList
template <typename Container>
void TestForwardTraversal(Container &container, const typename Container::value_type &threshold) {
    using Node      = typename Container::Node;
    using ValueType = typename Container::value_type;

    cout << "Probar call con std::cout";
    container.call(PrintNode<Node>, cout);
    cout << endl;

    cout << "Probar iterador crudo para impresión:";
    for (auto &&node : container)
        cout << node << " ";
    cout << endl;

    Node &found = container.FirstThat(IsGreaterThan<Node, ValueType>, threshold);
    cout << "FirstThat (primer valor > " << threshold << "): " << found << endl;
}

void DemoVector() {
    cout << styled(" █ Pruebas del contenedor Vector ").pure_black().bg_bright_yellow() << "\n\n";
    // Dejamos los archivos vacios para que TestContainer acumule (append)
    // el estado del container tras cada paso
    ofstream("vector.txt", ios::trunc).close();

    // Cada elemento es una pareja (valor, ref) que e guarda en un Node;
    // el constructor initializer_list arma el Vector inicial de una vez
    Vector<VectorAscTraits<TX>> vec({{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});
    TestContainer(vec, {{5, 15}, {6, 16}, {7, 17}, {8, 18}, {9, 19}}, "vector.txt");
    TestTraversal(vec, 5);

    ofstream("vector_str.txt", ios::trunc).close();
    Vector<VectorAscTraits<string>> strVec;
    TestContainer(strVec, {{"Hello", 1}, {"World", 2}}, "vector_str.txt");
    TestTraversal(strVec, "Hello");
}


// Callable
template <typename V, typename PrinterType>
concept IsCallableWith = requires(const V& v, PrinterType p) { v.call(p, std::cout); };
// Searchable
template <typename V, typename PredType, typename KeyType>
concept IsSearchableWith = requires(const V& v, PredType pred, const KeyType& key) { v.FirstThat(pred, key); };
// Applicable
template <typename V, typename MapperFunc>
concept IsApplicableWith = requires(V& v, MapperFunc f) { v.ApplyFunction(f); };
// Printable
template <typename V>
concept IsPrintable = requires(std::ostream& os, const V& v) { os << v; };

void TestCustomVector() {
    BaseVector<VectorAscTraits<TX>, Callable, Applicable, Searchable, Reversible> 
        vec({{0, 10}, {1, 11}, {2, 12}, {3, 13}, {4, 14}});
    auto rev = vec.reversed();
    using RevView = decltype(rev);
    using Node    = typename RevView::Node;

    cout << "Si a continuación no hay mensajes de error, "
         << "el contenedor ajustó sus mixins a lo pedido por el cliente." << endl;

    // Validamos Callable
    static_assert(
        IsCallableWith<RevView, decltype(&PrintNode<Node>)>,
        "Test Fallido: RevView debería ser Callable."
    );

    // Validamos Searchable
    static_assert(
        IsSearchableWith<RevView, decltype(&IsGreaterThan<Node, TX>), TX>,
        "Test Fallido: RevView debería ser Searchable con claves TX."
    );

    // Verificamos que Applicable y Printable fallan exitosamente
    static_assert(
        !IsApplicableWith<RevView, decltype(&AddOne)>,
        "Test Fallido: RevView NO debería permitir ApplyFunction."
    );
    static_assert(
        !IsPrintable<RevView>,
        "Test Fallido: RevView NO debería conservar el comportamiento Printable."
    );
}


void DemoCustomContainer() {
    TestCustomVector();
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
    for (size_t t = 0; t < NThreads; ++t) {
        workers.emplace_back([&vec, &values, n, t](){
            for (size_t i = t; i < n; i += NThreads)
                vec.push_back(values[i].first, values[i].second);
        });
    }
    for (auto &worker : workers)
        worker.join();

    TXX expectedSum = 0;
    for (auto &v : values) expectedSum += v.first;

    TXX actualSum = 0;
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

template <typename Container>
void TestLinkedList(Container &list, const string &filename) {
    init_directories();
    std::filesystem::path file_path = OUTPUT_DIR_PATH / filename;
    ofstream(file_path, ios::trunc).close();

    // Pruebas de push_back
    TestContainer(list, {{5, 15}, {1, 11}, {8, 18}, {3, 13}}, filename);
    TestForwardTraversal(list, 5);

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
    ifstream in(file_path);
    in >> new_list;
    in.close();
    cout << "Lista leida desde archivo: " << new_list << endl;
    TestForwardTraversal(new_list, 5);

    list = new_list;
}

template <typename DoubleContainer>
void TestDoubleLinkedListSpecifics(DoubleContainer &list) {
    TestTraversal(list, 5);

    list.clear();
    list.insert(5, 50);
    list.insert(1, 10);
    list.insert(9, 90);
    list.insert(6, 60);
    cout << "Lista tras inserts en cabeza, cola y medio: " << list << endl;
    TestTraversal(list, 5);
}

void DemoLinkedList()
{
    cout << "\n";
    cout << styled(" █ Pruebas para LinkedList ").pure_black().bg_bright_yellow() << "\n\n";
    LinkedList<LinkedListAscTraits<TX>> list;
    TestLinkedList(list, "linked_list.txt");

    cout << "\n";
    cout << styled(" █ Pruebas para DoubleLinkedList ").pure_black().bg_bright_yellow() << "\n\n";
    DoubleLinkedList<DoubleLinkedListAscTraits<TX>> dlist;
    TestLinkedList(dlist, "double_linked_list.txt");
    TestDoubleLinkedListSpecifics(dlist);

    cout << "\n";
    cout << styled(" █ Pruebas para CircularLinkedList ").pure_black().bg_bright_yellow() << "\n\n";
    CircularLinkedList<LinkedListAscTraits<TX>> clist;
    TestLinkedList(clist, "circular_linked_list.txt");

    cout << styled(" ◉ Diagrama de Lista Enlazada Circular (Personalizado con Lambda y Color): ").pure_black().bg_bright_cyan() << "\n";
    istringstream ss("[(51,15),(1,11),(8,18),(3,13),(45,16),(71,1),(28,8),(32,3),(15,5),(51,15),(1,11),(8,18),(15,5),(10,21),(81,28),(13,23)]");
    ss >> clist;
    clist.draw(
        CircularListLayout{[](const auto& node) {
            auto val = node.getValue();
            if (val > 50) {
                return styled(val).bright_red().bold().str();
            } else if (val % 2 == 0) {
                return styled(val).bright_green().bold().str();
            } else {
                return styled(val).bright_yellow().str();
            }
        }},
        ConsoleGraphRenderer(cout)
    );
    cout << endl;

    cout << "\n";
    cout << styled(" █ Pruebas para CircularDoubleLinkedList ").pure_black().bg_bright_yellow() << "\n\n";
    CircularDoubleLinkedList<DoubleLinkedListAscTraits<TX>> cdlist;
    TestLinkedList(cdlist, "circular_double_linked_list.txt");
    TestDoubleLinkedListSpecifics(cdlist);

    cout << styled(" ◉ Diagrama de Lista Circular Doblemente Enlazada (Truecolor RGB / Cyan): ").pure_black().bg_bright_cyan() << "\n";
    cdlist.draw(
        CircularListLayout{[](const auto& node) {
            auto val = node.getValue();
            return styled(val).rgb(0, 220, 255).bold().str();
        }},
        ConsoleGraphRenderer(cout)
    );
    cout << endl;
}

template <typename Heap>
void InitHeap(Heap &heap) {
    heap.push(50, 5);
    heap.push(20, 2);
    heap.push(80, 8);
    heap.push(10, 1);
    heap.push(30, 3);
    heap.push(70, 7);
    cout << "Tras inserciones: " << heap << endl;
}

template <typename Heap>
void ExtractHeap(Heap &heap) {
    cout << "Tras extracción: ";
    while (!heap.empty()) {
        auto node = heap.pop();
        cout << node.getValue() << " ";
    }
    cout << endl;
}

template <typename Heap>
void TestHeap(Heap& heap) {
    InitHeap(heap);
    ExtractHeap(heap);
    InitHeap(heap);
    heap.ApplyFunction(AddOne);
    cout << "Tras aplicar AddOne: " << heap << endl;
    TX m = 13;
    heap.ApplyFunction(ModX<TX>, TX(m));
    cout << "Tras aplicar mod " << m << ": " << heap << endl;
    ExtractHeap(heap);
    cout << endl;
}

void DemoHeap() {
    cout << "\n";
    cout << styled(" █ Pruebas para MinHeap ").pure_black().bg_bright_yellow() << "\n\n";
    MinHeap minHeap;
    TestHeap(minHeap);

    cout << "\n";
    cout << styled(" █ Pruebas para MaxHeap ").pure_black().bg_bright_yellow() << "\n\n";
    MaxHeap maxHeap;
    TestHeap(maxHeap);
}

// Función que calcula un color pseudo-aleatorio consistente basado en operaciones modulares sobre el valor del nodo
template <typename T>
std::string get_modular_node_color(const T& val) {
    static const Color palette[] = {
        Color::BrightRed,
        Color::BrightGreen,
        Color::BrightYellow,
        Color::BrightBlue,
        Color::BrightMagenta,
        Color::BrightCyan,
        Color::Cyan,
        Color::Green
    };
    constexpr size_t N = sizeof(palette) / sizeof(palette[0]);
    size_t idx = static_cast<size_t>(((val * 5 + 3) % N + N) % N);
    return styled(val).add(palette[idx]).bold().str();
}

// contenedor instanciado como tipo concreto
template < typename TreeContainer >
void TestBinaryTreeGeneral(TreeContainer &tree, const string &filename) {
    init_directories();
    std::filesystem::path file_path = OUTPUT_DIR_PATH / filename;
    ofstream(file_path, ios::trunc).close();

    using ValueType = typename TreeContainer::value_type;


    // Pruebas de insercion y persistencia
    TestContainer(tree, 
        {
            {50, 5},  {20, 2},  {80, 8},  {10, 1},   {30, 3},  {70, 7},  {90, 9},
            {55, 10}, {25, 12}, {85, 18}, {100, 14}, {35, 13}, {75, 17}, {95, 19},
            {43, 20}, {33, 22}, {73, 28}, {93, 24},  {83, 23}, {71, 27}, {63, 29},
            {96, 30}
        }, 
        filename);

    cout << styled(" ◉ Diagrama de árbol (Disposición Vertical con Roles Semánticos / CSS): ").pure_black().bg_bright_cyan() << "\n";
    std::cout << "(Altura: " << tree.height() << ")" << std::endl;
    tree.draw(
        VerticalTreeLayout{[](const auto& node, TreeRole role) {
            auto val = node.getValue();
            switch (role) {
                case TreeRole::Root:  return styled(val).bright_magenta().bold().underline().str();
                case TreeRole::Leaf:  return styled(val).bright_yellow().bold().str();
                case TreeRole::Left:  return styled(val).bright_cyan().str();
                case TreeRole::Right: return styled(val).bright_green().str();
            }
            return styled(val).str();
        }},
        ConsoleGraphRenderer(cout)
    );
    std::cout << std::endl;

    cout << styled(" ◉ Diagrama de árbol (Disposición Horizontal - Dirigida): ").pure_black().bg_bright_cyan() << "\n";
    tree.draw(HorizontalTreeLayout{true}, ConsoleGraphRenderer(cout));
    std::cout << std::endl;

    cout << styled(" ◉ Diagrama de árbol (Disposición Horizontal - No dirigida): ").pure_black().bg_bright_cyan() << "\n";
    tree.draw(HorizontalTreeLayout{false}, ConsoleGraphRenderer(cout));
    std::cout << std::endl;

    // in-order
    TestTraversal(tree, 50);

    // Pruebas de eliminacion (hoja, 1 hijo, 2 hijos / raiz)
    using Node = typename TreeContainer::Node;
    vector<ValueType> to_remove = {10, 55, 20, 80, 50};
    for (const auto &val : to_remove) {
        Node &target_node = tree.FirstThat(IsEqualTo<Node, ValueType>, val);
        tree.remove(&target_node);
        string s = "Arbol tras remove(" + std::to_string(val) + "): ";
        cout << styled(" ◉ " + s).pure_black().bg_bright_red() << "\n";
        std::cout << tree << endl;
        cout << styled(" ◉ Diagrama de árbol tras eliminaciones (Colores Modulares): ").pure_black().bg_bright_red() << "\n";
        std::cout << "(Altura: " << tree.height() << ")" << std::endl;
        tree.draw(
            VerticalTreeLayout{[](const auto& node) {
                return get_modular_node_color(node.getValue());
            }},
            ConsoleGraphRenderer(cout)
        );
        std::cout << std::endl;
    }


    tree.ApplyFunction(AddOne);
    cout << "Arbol tras usar AddOne: " << tree << endl;
    tree.ApplyFunction(AddX< ValueType >, ValueType(10));
    cout << "Arbol tras usar AddX(10): " << tree << endl;

    tree.clear();
    cout << "Arbol luego de usar clear: " << tree << endl;

    // lectura
    TreeContainer new_tree;
    ifstream in(file_path);
    in >> new_tree;
    in.close();
    cout << "Arbol leido desde archivo: " << new_tree << endl;

    tree = new_tree;
    cout << "Arbol tras operator=: " << tree << endl;
}

template < Order Order, typename Tree >
void TestOneOrderTour(const Tree &tree) {
    cout << "- Ascendente:  " << tree.template traverse<Order, Direction::Asc>()  << endl;
    cout << "- Descendente: " << tree.template traverse<Order, Direction::Desc>() << endl;
}

template < template < typename > class BinaryTree >
void TestAllTours() {
    PrintColored(cout, "Probar los 14 recorridos posibles:", Color::Green);
    
    // Instanciamos un unico arbol y consultamos sus 14 vistas en O(1)
    using DefaultTree = BinaryTree<BinaryTreeAscTraits<TX>>;
    using Node        = typename DefaultTree::Node;
    DefaultTree tree;

    vector< pair< typename DefaultTree::value_type, Ref > > values = {
        {50, 5}, {20, 2}, {80, 8}, {10, 1}, {30, 3}, {70, 7}, {90, 9}
    };

    InsertElements(tree, values);
    cout << "(Altura: " << tree.height() << ")" << std::endl;
    tree.draw(VerticalTreeLayout{}, ConsoleGraphRenderer(cout));
    cout << endl;

    cout << "Recorrido in-order:" << endl;
    TestOneOrderTour<Order::InOrder>(tree);
    
    cout << "Recorrido pre-order:" << endl;
    TestOneOrderTour< Order::PreOrder>(tree);

    cout << "Recorrido post-order:" << endl;
    TestOneOrderTour<Order::PostOrder>(tree);

    cout << "Recorrido pre-in-order:" << endl;
    TestOneOrderTour<Order::PreInOrder>(tree);
    
    cout << "Recorrido pre-post-order:" << endl;
    TestOneOrderTour<Order::PrePostOrder>(tree);

    cout << "Recorrido in-post-order:" << endl;
    TestOneOrderTour<Order::InPostOrder>(tree);

    cout << "Recorrido pre-in-post-order:" << endl;
    TestOneOrderTour<Order::PreInPostOrder>(tree);

    cout << "\nPruebas adicionales de Mixins (call y FirstThat) sobre vistas .traverse<>():" << endl;
    cout << "- PreOrder con .call(PrintNode): [";
    tree.template traverse< Order::PreOrder >().call(PrintNode< Node >, cout);
    cout << "]" << endl;

    Node &post_first = tree.template traverse< Order::PostOrder >().FirstThat(IsGreaterThan< Node, TX >, TX(25));
    cout << "- PostOrder .FirstThat(> 25): " << post_first << endl;

    Node &euler_desc_first = tree.template traverse< Order::PreInPostOrder, Direction::Desc >().FirstThat(IsEqualTo< Node, TX >, TX(70));
    cout << "- PreInPostOrder (Desc) .FirstThat(== 70): " << euler_desc_first << endl;
}

template < template < typename > class TreeContainer >
void TestBinaryTree(const string &filename = "tree_test.txt") {
    init_directories();
    std::filesystem::path file_path = OUTPUT_DIR_PATH / filename;
    // instanciación con traits por defecto
    TreeContainer< BinaryTreeAscTraits< TX > > tree;
    
    TestBinaryTreeGeneral(tree, filename);
    
    // prueba de recorridos parametrizando la plantilla
    TestAllTours< TreeContainer >();
}

void DemoBinaryTree() {
    cout << styled(" █ Pruebas para BinaryTree ").pure_black().bg_bright_yellow() << "\n\n";
    TestBinaryTree<BinaryTree>("binarytree_test.txt");
    cout << endl;
    
    cout << styled(" █ Pruebas para AVLTree ").pure_black().bg_bright_yellow() << "\n\n";
    TestBinaryTree<AVLTree>("avltree_test.txt");
}

void DemoHashTable() {
    cout << styled(" █ Pruebas para HashTable ").pure_black().bg_bright_yellow() << "\n\n";

    HashTable<HashTableTraits<std::string, TX>> htable;
    htable["a"] = 42;
    htable["b"] = 7;
    htable["c"] = 18;
    htable["la hermenéutica telúrica incáica"] = 9;

    cout << "HashTable usando operator<< (Printable):" << endl;
    cout << htable << endl << endl;

    cout << "Acceso con operator[]:" << endl;
    for (const auto& pair : htable) {
        auto key = pair.getValue().getKey();
        cout << "- Clave " << quoted(key) << ": " << htable[key] << endl;
    }
    cout << "Consultar si existe:" << endl;
    cout << "- contains('b'): " << (htable.contains("b") ? "true" : "false") << endl;
    cout << "- contains('z'): " << (htable.contains("z") ? "true" : "false") << endl << endl;

    // Prueba de persistencia (escritura en archivo y lectura con Readable)
    filesystem::path file_path = OUTPUT_DIR_PATH / "hashtable_test.txt";
    ofstream of(file_path , ios::trunc);
    of << htable << endl;
    of.close();

    HashTable<HashTableTraits<std::string, TX>> htable_restored;
    ifstream in(file_path );
    in >> htable_restored;
    in.close();

    PrintColored(cout, "HashTable leido desde archivo (Readable):", Color::Green);
    cout << htable_restored << endl;
    cout << "- Clave 'la hermenéutica telúrica incáica' restaurada: " 
         << htable_restored["la hermenéutica telúrica incáica"] << endl;
}

void DemoMerkleTree() {
    cout << "\n";
    cout << styled(" █ Pruebas para MerkleTree ").pure_black().bg_bright_yellow() << "\n\n";

    MerkleTree<> mtree;
    mtree.push_back("Tx0: Alice -> Bob 50 BTC", 0);
    mtree.push_back("Tx1: Bob -> Charlie 20 BTC", 1);
    mtree.push_back("Tx2: Charlie -> Dave 10 BTC", 2);
    mtree.push_back("Tx3: Dave -> Eve 5 BTC", 3);

    cout << "Cantidad de hojas: " << mtree.size() << endl;
    cout << "Altura del arbol:  " << mtree.height() << endl;
    cout << "Merkle Root Hash:  " << mtree.getRootHash() << endl << endl;

    cout << "Arbol Merkle usando operator<< (Printable - Recorrido In-Order):" << endl;
    cout << mtree << endl << endl;

    // Formateador inyectado por el cliente para pintar el árbol según sus roles semánticos
    auto merkle_painter = [](const std::string& text, MerkleRole role) {
        switch (role) {
            case MerkleRole::Root:      return styled(text.substr(0, 8)).bold().bright_green().str();
            case MerkleRole::Leaf:      return styled(text.substr(0, 8)).bold().bright_yellow().str();
            case MerkleRole::CloneLeaf: return styled(text.substr(0, 8)).dim().italic().str();
            case MerkleRole::Internal:  return styled(text.substr(0, 8)).dim().str();
            case MerkleRole::Data:      return styled("[" + text + "]").bright_cyan().str();
            case MerkleRole::CloneData: return styled("[" + text + " (copia)]").dim().italic().str();
        }
        return text;
    };

    cout << "Visualizacion del Merkle Tree (4 transacciones - Pintado por el Cliente):" << endl;
    mtree.draw(MerkleTreeLayout{merkle_painter}, ConsoleGraphRenderer(cout));
    cout << endl;

    // Recorrido Pre-Order mediante Euler Tour
    cout << "Recorrido Pre-Order sobre MerkleTree (.traverse<Order::PreOrder>()):" << endl;
    cout << "[";
    mtree.traverse<Order::PreOrder>().call([](const auto& node, ostream& os) {
        os << "(" << node.getValue() << ", ref=" << node.getRef() << ") ";
    }, cout);
    cout << "]" << endl << endl;

    // Generar y verificar Merkle Proof para la transacción 2
    size_t target_idx = 2;
    string target_tx = mtree.getData(target_idx);
    auto proof = mtree.getProof(target_idx);

    PrintColored(cout, "Generacion de Merkle Proof para hoja #" + to_string(target_idx) + ":", Color::Cyan);
    cout << "Dato original: " << quoted(target_tx) << endl;
    cout << "Hash de hoja:  " << mtree.getLeafHash(target_idx) << endl;
    cout << "Pasos en el camino de auditoria (audit path):" << endl;
    for (size_t i = 0; i < proof.size(); ++i) {
        cout << "  Nivel " << i + 1 << ": " << proof[i].getValue() << endl;
    }
    cout << endl;

    // Verificación con datos auténticos
    bool valid = MerkleTree<>::verify(target_tx, proof, mtree.getRootHash());
    cout << "Verificacion con dato autentico: ";
    if (valid) {
        PrintColored(cout, "VALIDA (Integridad confirmada)", Color::Green);
    } else {
        PrintColored(cout, "INVALIDA", Color::Red);
    }
    cout << endl;

    // Verificación con datos manipulados / alterados
    string forged_tx = "Tx2: Charlie -> Dave 9999 BTC";
    bool forged_valid = MerkleTree<>::verify(forged_tx, proof, mtree.getRootHash());
    cout << "Verificacion con dato falsificado (" << quoted(forged_tx) << "): ";
    if (!forged_valid) {
        PrintColored(cout, "RECHAZADA (Ataque detectado con exito)", Color::Green);
    } else {
        PrintColored(cout, "FALLO DE SEGURIDAD", Color::Red);
    }
    cout << endl;

    // Actualización de una hoja y verificación de cambio en la raíz
    string old_root = mtree.getRootHash();
    cout << "Actualizando hoja #" << target_idx << "..." << endl;
    mtree.update(target_idx, "Tx2: Charlie -> Dave 15 BTC");
    string new_root = mtree.getRootHash();
    cout << "Raiz anterior: " << old_root << endl;
    cout << "Nueva raiz:    " << new_root << endl;
    cout << "¿Raices distintas? " << (old_root != new_root ? "Si (Propagacion correcta)" : "No") << endl;

    cout << "\nVisualizacion tras actualizar hoja #2 (nueva raiz propagada - Pintado por el Cliente):" << endl;
    mtree.draw(MerkleTreeLayout{merkle_painter}, ConsoleGraphRenderer(cout));
    cout << endl;

    // La prueba anterior ya no debe ser válida para la nueva raíz
    bool old_proof_on_new_root = MerkleTree<>::verify(target_tx, proof, new_root);
    cout << "¿Prueba vieja valida en nueva raiz? " 
         << (old_proof_on_new_root ? "Si" : "No (Correcto, estado invalidado)") << endl << endl;

    // Prueba de número impar de transacciones (duplicación balanceada)
    PrintColored(cout, "Prueba con cantidad impar de transacciones (5 hojas):", Color::Cyan);
    mtree.push_back("Tx4: Eve -> Frank 1 BTC", 4);
    cout << "Cantidad de hojas: " << mtree.size() << endl;
    cout << "Altura del arbol:  " << mtree.height() << endl;
    cout << "Nueva Merkle Root: " << mtree.getRootHash() << endl;

    auto proof_odd = mtree.getProof(4);
    bool valid_odd = MerkleTree<>::verify(mtree.getData(4), proof_odd, mtree.getRootHash());
    cout << "Verificacion de hoja impar (#4): " 
         << (valid_odd ? "VALIDA (OK)" : "INVALIDA") << endl;

    cout << "\nVisualizacion con 5 transacciones (Layout Puro sin Pintar - Texto Plano):" << endl;
    mtree.draw(MerkleTreeLayout{}, ConsoleGraphRenderer(cout));
    cout << endl;

    cout << "Visualizacion con 5 transacciones (Pintado por el Cliente con Roles Semanticos):" << endl;
    mtree.draw(MerkleTreeLayout{merkle_painter}, ConsoleGraphRenderer(cout));
    cout << endl;

    cout << "Visualizacion con Paleta Alternativa inyectada por el Cliente:" << endl;
    mtree.draw(
        MerkleTreeLayout{[](const std::string& text, MerkleRole role) {
            switch (role) {
                case MerkleRole::Root:      return styled(text.substr(0, 8)).bold().bright_magenta().str();
                case MerkleRole::Leaf:      return styled(text.substr(0, 8)).bold().bright_cyan().str();
                case MerkleRole::CloneLeaf: return styled(text.substr(0, 8)).dim().italic().str();
                case MerkleRole::Internal:  return styled(text.substr(0, 8)).dim().str();
                case MerkleRole::Data:      return styled("<" + text + ">").bright_yellow().bold().str();
                case MerkleRole::CloneData: return styled("<" + text + " [DUP]>").dim().italic().str();
            }
            return text;
        }},
        ConsoleGraphRenderer(cout)
    );
    cout << endl;

    // Prueba de persistencia (escritura en archivo y lectura con reconstrucción exacta)
    filesystem::path merkle_path = OUTPUT_DIR_PATH / "merkle_test.txt";
    ofstream mof(merkle_path, ios::trunc);
    mof << mtree << endl;
    mof.close();

    MerkleTree<> mtree_restored;
    ifstream min(merkle_path);
    min >> mtree_restored;
    min.close();

    PrintColored(cout, "MerkleTree leido desde archivo (Persistencia):", Color::Green);
    cout << mtree_restored << endl;
    cout << "- Merkle Root original:   " << mtree.getRootHash() << endl;
    cout << "- Merkle Root restaurada: " << mtree_restored.getRootHash()
         << (mtree.getRootHash() == mtree_restored.getRootHash() ? " (IDENTICAS)" : " (DIFERENTES)")
         << endl;
}


// Demos de BTree y BStarTree

namespace {

const vector<pair<TX, Ref>> BTreeSample = {
    {50, 1}, {20, 2}, {80, 3}, {10, 4}, {30, 5}, {70, 6}, {90, 7}, {5, 8},
    {15, 9}, {25, 10}, {35, 11}, {60, 12}, {75, 13}, {85, 14}, {95, 15}
};

const vector<TX> BTreeSmall = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};

void Negate(GeneralNode<TX>& node) { node.value() = -node.value(); }

void Subtitle(const string& text) {
    cout << styled(" ◉ " + text + " ").pure_black().bg_bright_cyan() << "\n";
}

// --- Demo ---

class DemoSection {
public:
    virtual ~DemoSection() = default;

    void execute() const {
        cout << styled(" █ " + string(title()) + " ").pure_black().bg_bright_yellow() << "\n\n";
        run();
        cout << "\n";
    }

protected:
    virtual string_view title() const = 0;
    virtual void run() const = 0;
};


class TreeView {
public:
    virtual ~TreeView() = default;

    virtual void show(const VisualGraph& graph, const string& caption) const = 0;
    virtual void compare(const VisualGraph& left, const string& leftCaption,
                         const VisualGraph& right, const string& rightCaption) const = 0;

    template <typename Tree>
    static VisualGraph project(const Tree& tree, const BTreePageSet<typename Tree::Page>& pages = {}) {
        return BTreeLayout{highlight_pages(pages)}(tree);
    }

    template <typename Tree>
    void draw(const Tree& tree, const string& caption, const BTreePageSet<typename Tree::Page>& pages = {}) const {
        show(project(tree, pages), caption);
    }
};

class GraphView : public TreeView {
public:
    void show(const VisualGraph& graph, const string& caption) const override {
        cout << caption << "\n";
        vector<string> lines = console_lines(graph);
        if (lines.empty()) cout << "(arbol vacio)\n";
        for (const string& line : lines) cout << line << "\n";
    }

    void compare(const VisualGraph& left, const string& leftCaption,
                 const VisualGraph& right, const string& rightCaption) const override {
        show(left, leftCaption);
        show(right, rightCaption);
    }
};

class SideBySideView final : public GraphView {
public:
    void compare(const VisualGraph& left, const string& leftCaption,
                 const VisualGraph& right, const string& rightCaption) const override {
        cout << side_by_side(left, leftCaption, right, rightCaption);
    }
};

// --- Instrumented<Tree> ---

enum class BalanceOp {
    RootGrowth, RootSplit13, Split12, Split23,
    RedistributeLeft, RedistributeRight, BorrowLeft, BorrowRight, Redistribute3,
    Merge21, Merge32, RootContraction, RootAbsorb, Unknown
};

string_view describe(BalanceOp op) {
    switch (op) {
        case BalanceOp::RootGrowth:        return "crecimiento de raíz (división 1→2 de la raíz)";
        case BalanceOp::RootSplit13:       return "división de raíz 1→3 (SplitRoot)";
        case BalanceOp::Split12:           return "división 1→2";
        case BalanceOp::Split23:           return "división 2→3";
        case BalanceOp::RedistributeLeft:  return "redistribución hacia el hermano izquierdo";
        case BalanceOp::RedistributeRight: return "redistribución hacia el hermano derecho";
        case BalanceOp::BorrowLeft:        return "préstamo del hermano izquierdo";
        case BalanceOp::BorrowRight:       return "préstamo del hermano derecho";
        case BalanceOp::Redistribute3:     return "redistribución entre 3 hermanos";
        case BalanceOp::Merge21:           return "fusión 2→1";
        case BalanceOp::Merge32:           return "fusión 3→2";
        case BalanceOp::RootContraction:   return "contracción de raíz";
        case BalanceOp::RootAbsorb:        return "absorción de la raíz (MergeRoot)";
        case BalanceOp::Unknown:           return "operación no reconocida";
    }
    return "operación no reconocida";
}

template <typename Tree>
class Instrumented final : public Tree {
public:
    using Tree::Tree;
    using typename Tree::Page;
    using typename Tree::PagePtr;
    using Keys = vector<typename Tree::value_type>;

    const vector<BalanceOp>& events() const { return m_events; }
    void clearEvents() { m_events.clear(); }
    bool happened(BalanceOp op) const { return std::find(m_events.begin(), m_events.end(), op) != m_events.end(); }

protected:
    void handle_overflow(PagePtr page) override {
        Snapshot before = snapshot(page);
        Tree::handle_overflow(page);
        m_events.push_back(classify(before, true));
    }

    void handle_underflow(PagePtr page) override {
        Snapshot before = snapshot(page);
        Tree::handle_underflow(page);
        m_events.push_back(classify(before, false));
        if (m_events.back() == BalanceOp::Merge21 && before.parent->isRoot() && before.parent->keyCount() == 0)
            m_events.push_back(BalanceOp::RootContraction);   // shrink_root la contrae
    }

private:
    struct Snapshot {
        PagePtr      parent;
        Index        pos;
        Size         parentKeys;
        Size         height;
        vector<Keys> children;
    };

    vector<BalanceOp> m_events;

    static Keys keysOf(const Page& page) {
        Keys keys;
        for (Index i{}; i < page.keyCount(); ++i) keys.push_back(page.key(i).getValue());
        return keys;
    }

    static vector<Keys> childrenOf(const Page& parent) {
        vector<Keys> out;
        for (Index i{}; i < parent.childCount(); ++i) out.push_back(keysOf(*parent.child(i)));
        return out;
    }

    Snapshot snapshot(PagePtr page) const {
        PagePtr parent = page->parent();
        return {parent, *page->positionInParent(), parent->keyCount(), this->height(), childrenOf(*parent)};
    }

    BalanceOp classify(const Snapshot& before, bool overflow) const {
        vector<Keys>  after = childrenOf(*before.parent);
        vector<Index> changed;
        for (Index i{}; i < before.children.size(); ++i)
            if (std::find(after.begin(), after.end(), before.children[i]) == after.end()) changed.push_back(i);
        auto touched = [&](Index i) { return std::find(changed.begin(), changed.end(), i) != changed.end(); };
        bool grew   = after.size() == before.children.size() + 1;
        bool same   = after.size() == before.children.size();
        bool shrank = after.size() + 1 == before.children.size();
        bool left   = before.pos > 0 && touched(before.pos - 1);
        bool right  = touched(before.pos + 1);

        if (overflow) {
            if (before.parentKeys == 0 && before.children.size() == 1)
                return after.size() == 3 ? BalanceOp::RootSplit13 : BalanceOp::RootGrowth;
            if (grew)          return changed.size() == 1 ? BalanceOp::Split12 : BalanceOp::Split23;
            if (same && left)  return BalanceOp::RedistributeLeft;
            if (same && right) return BalanceOp::RedistributeRight;
            return BalanceOp::Unknown;
        }
        if (before.parent->isRoot() && this->height() < before.height) return BalanceOp::RootAbsorb;
        if (shrank) return changed.size() == 3 ? BalanceOp::Merge32 : BalanceOp::Merge21;
        if (same && changed.size() == 2 && left)  return BalanceOp::BorrowLeft;
        if (same && changed.size() == 2 && right) return BalanceOp::BorrowRight;
        if (same) return BalanceOp::Redistribute3;
        return BalanceOp::Unknown;
    }
};

// --- Scenario<Tree> 

template <typename Tree>
class Scenario {
public:
    Scenario(string name, string why, Size order, vector<TX> setup, TX key, BalanceOp expected)
        : m_name(std::move(name)), m_why(std::move(why)), m_order(order),
          m_setup(std::move(setup)), m_key(key), m_expected(expected) {}
    virtual ~Scenario() = default;

    const string& name() const { return m_name; }
    BalanceOp expected() const { return m_expected; }

    void play(const TreeView& view) const {
        Instrumented<Tree> tree(m_order);
        prepare(tree);
        tree.clearEvents();
        cout << styled("▸ " + m_name).bold() << " — " << m_why << "\n";
        VisualGraph before = TreeView::project(tree, btree_search_path(tree, m_key));
        apply(tree);
        VisualGraph after = TreeView::project(tree, affected(tree));
        view.compare(before, "Antes: " + action(), after, "Despues");

        string seen;
        for (BalanceOp op : tree.events()) seen += "[" + string(describe(op)) + "] ";
        if (!tree.happened(m_expected))
            throw std::logic_error("Escenario '" + m_name + "': se esperaba " + string(describe(m_expected)) + " y ocurrio " + seen);
        cout << "  operaciones: " << seen << "→ " << styled("✔ " + string(describe(m_expected))).bright_green() << "\n\n";
    }

protected:
    virtual void prepare(Instrumented<Tree>& tree) const {
        Ref ref = 1;
        for (TX value : m_setup) tree.insert(value, ref++);
    }
    virtual void apply(Instrumented<Tree>& tree) const = 0;
    virtual string action() const = 0;
    virtual BTreePageSet<typename Tree::Page> affected(const Instrumented<Tree>& tree) const = 0;

    TX key() const { return m_key; }

private:
    string     m_name;
    string     m_why;
    Size       m_order;
    vector<TX> m_setup;
    TX         m_key;
    BalanceOp  m_expected;
};

template <typename Tree>
class InsertScenario final : public Scenario<Tree> {
public:
    using Scenario<Tree>::Scenario;
protected:
    void apply(Instrumented<Tree>& tree) const override { tree.insert(this->key(), 0); }
    string action() const override { return "insert(" + std::to_string(this->key()) + ")"; }
    BTreePageSet<typename Tree::Page> affected(const Instrumented<Tree>& tree) const override {
        return btree_page_of(tree, this->key());
    }
};

template <typename Tree>
class RemoveScenario final : public Scenario<Tree> {
public:
    using Scenario<Tree>::Scenario;
protected:
    void apply(Instrumented<Tree>& tree) const override { tree.remove(this->key()); }
    string action() const override { return "remove(" + std::to_string(this->key()) + ")"; }
    BTreePageSet<typename Tree::Page> affected(const Instrumented<Tree>& tree) const override {
        return btree_search_path(tree, this->key());
    }
};

vector<TX> Tens(TX n) {
    vector<TX> keys;
    for (TX i = 1; i <= n; ++i) keys.push_back(10 * i);
    return keys;
}

// --- TreeFamilyDemo<Family>

template <template <typename> typename Family>
class TreeFamilyDemo : public DemoSection {
protected:
    using Tree      = Family<BTreeAscTraits<TX>>;
    using DescTree  = Family<BTreeDescTraits<TX>>;
    using Scenarios = vector<unique_ptr<Scenario<Tree>>>;

    static constexpr Size Order = Tree::MinOrder;

    virtual Scenarios scenarios() const = 0;

    void run() const final {
        Tree tree(Order);
        construction(tree);
        search(tree);
        traversal(tree);
        apply(tree);
        copyAndMove();
        persistence(tree);
        traits();
        balancing();
    }

private:
    SideBySideView m_view;

    static string stem() { return "btree_orden" + std::to_string(Order); }

    template <typename T>
    static T smallTree() {
        T tree(Order);
        Ref ref = 1;
        for (TX value : BTreeSmall) tree.insert(value, ref++);
        return tree;
    }

    void construction(Tree& tree) const {
        Subtitle("Construccion con TestContainer (orden " + std::to_string(Order) + ")");
        init_directories();
        ofstream(OUTPUT_DIR_PATH / (stem() + ".txt"), ios::trunc).close();
        TestContainer(tree, BTreeSample, stem() + ".txt");
        m_view.draw(tree, "Arbol resultante: altura " + std::to_string(tree.height()) + ", " + std::to_string(tree.size()) + " claves");
        cout << "\n";
    }

    void search(const Tree& tree) const {
        Subtitle("Busqueda (camino resaltado)");
        auto present = btree_search_path(tree, 25);
        auto absent  = btree_search_path(tree, 42);
        auto visited = [&](Size pages) {
            return "paginas visitadas = " + std::to_string(pages) + (pages == tree.height() ? " = altura " : " < altura ")
                 + std::to_string(tree.height());
        };
        cout << "find(25) -> " << (tree.find(25) ? "encontrada en una hoja" : "ausente") << "; " << visited(present.size()) << "\n";
        cout << "find(42) -> " << (tree.find(42) ? "encontrada" : "nullptr") << "; " << visited(absent.size()) << "\n";
        m_view.draw(tree, "Camino de busqueda de 25", present);
        cout << "\n";
    }

    void traversal(Tree& tree) const {
        Subtitle("Recorridos con TestTraversal");
        TestTraversal(tree, 50);
        cout << "\n";
    }

    void apply(Tree& tree) const {
        Subtitle("ApplyFunction (AddOne, AddX y negacion con rebuild)");
        tree.ApplyFunction(AddOne);
        cout << "Tras AddOne:   " << tree << "\n";
        tree.ApplyFunction(AddX<TX>, TX{10});
        cout << "Tras AddX(10): " << tree << "\n";
        Tree small = smallTree<Tree>();
        VisualGraph before = TreeView::project(small);
        small.ApplyFunction(Negate);
        m_view.compare(before, "Antes de negar", TreeView::project(small), "Tras negar: rebuild reorganiza");
        cout << "\n";
    }

    void copyAndMove() const {
        Subtitle("Copia estructural y movimiento");
        Tree original = smallTree<Tree>();
        Tree copy(original);
        copy.remove(40);
        copy.insert(45, 9);
        m_view.compare(TreeView::project(original), "Original (intacto)",
                       TreeView::project(copy, btree_page_of(copy, 45)), "Copia tras remove(40), insert(45)");
        Tree moved(std::move(copy));
        cout << "Tras mover la copia: destino con " << moved.size() << " claves; origen con "
             << copy.size() << " claves (vacio: " << (copy.empty() ? "si" : "no") << ")\n\n";
    }

    void persistence(const Tree& tree) const {
        Subtitle("Persistencia (<< / >>) leyendo en otro orden");
        filesystem::path path = OUTPUT_DIR_PATH / (stem() + "_persistencia.txt");
        ofstream out(path, ios::trunc);
        out << tree << endl;
        out.close();
        Tree restored(Order + 2);
        ifstream in(path);
        in >> restored;
        in.close();
        bool same = std::equal(tree.begin(), tree.end(), restored.begin(), restored.end(),
            [](const auto& a, const auto& b) { return a.getValue() == b.getValue() && a.getRef() == b.getRef(); });
        cout << "Escrito en " << path.string() << " y leido en un arbol de orden " << restored.order()
             << ": " << restored.size() << " claves, mismo recorrido: " << (same ? "si" : "no") << "\n";
        m_view.draw(restored, "Arbol leido (orden " + std::to_string(restored.order()) + ")");
        cout << "\n";
    }

    void traits() const {
        Subtitle("Traits: BTreeAscTraits vs BTreeDescTraits");
        Tree asc = smallTree<Tree>();
        DescTree desc = smallTree<DescTree>();
        m_view.compare(TreeView::project(asc), "Asc (std::less)", TreeView::project(desc), "Desc (std::greater)");
        cout << "\n";
    }

    void balancing() const {
        Subtitle("Escenarios de balanceo (verificados con Instrumented)");
        for (const auto& scenario : scenarios()) scenario->play(m_view);
    }
};

class BTreeDemo final : public TreeFamilyDemo<BTree> {
protected:
    string_view title() const override { return "Pruebas para BTree (arbol B clasico)"; }

    Scenarios scenarios() const override {
        using Insert = InsertScenario<Tree>;
        using Remove = RemoveScenario<Tree>;
        Scenarios list;
        list.push_back(make_unique<Insert>("Division 1->2", "hoja llena con padre con espacio",
            Order, Tens(8), 65, BalanceOp::Split12));
        list.push_back(make_unique<Insert>("Crecimiento de raiz", "la division se propaga hasta una raiz llena",
            Order, Tens(6), 45, BalanceOp::RootGrowth));
        list.push_back(make_unique<Remove>("Prestamo del hermano izquierdo", "el hermano izquierdo tiene una clave de sobra",
            Order, vector<TX>{10, 20, 50, 30, 60, 40}, 60, BalanceOp::BorrowLeft));
        list.push_back(make_unique<Remove>("Prestamo del hermano derecho", "solo el hermano derecho puede prestar",
            Order, Tens(6), 30, BalanceOp::BorrowRight));
        list.push_back(make_unique<Remove>("Fusion 2->1", "ningun hermano adyacente puede prestar",
            Order, Tens(6), 10, BalanceOp::Merge21));
        list.push_back(make_unique<Remove>("Contraccion de raiz", "las fusiones en cascada vacian la raiz",
            Order, Tens(7), 10, BalanceOp::RootContraction));
        return list;
    }
};

class BStarTreeDemo final : public TreeFamilyDemo<BStarTree> {
protected:
    string_view title() const override { return "Pruebas para BStarTree (arbol B*)"; }

    Scenarios scenarios() const override {
        using Insert = InsertScenario<Tree>;
        using Remove = RemoveScenario<Tree>;
        Scenarios list;
        list.push_back(make_unique<Insert>("Redistribucion hacia un hermano", "hoja llena con un hermano con espacio",
            Order, Tens(9), 65, BalanceOp::RedistributeLeft));
        list.push_back(make_unique<Insert>("Division 2->3", "la hoja y sus hermanos estan llenos",
            Order, Tens(10), 75, BalanceOp::Split23));
        list.push_back(make_unique<Insert>("Division de raiz 1->3", "la raiz hoja supera R = 7 claves",
            Order, Tens(7), 5, BalanceOp::RootSplit13));
        list.push_back(make_unique<Remove>("Redistribucion entre 3 hermanos", "la ventana de 3 hermanos tiene >= 3L claves",
            Order, Tens(9), 10, BalanceOp::Redistribute3));
        list.push_back(make_unique<Remove>("Fusion 3->2", "3 hermanos sin claves suficientes para repartir",
            Order, Tens(11), 10, BalanceOp::Merge32));
        list.push_back(make_unique<Remove>("Absorcion de la raiz (MergeRoot)", "la raiz con pocos hijos al minimo los absorbe",
            Order, Tens(8), 10, BalanceOp::RootAbsorb));
        return list;
    }
};

// --- ComparisonDemo ---

class ComparisonDemo final : public DemoSection {
protected:
    string_view title() const override { return "Comparacion BTree vs BStarTree"; }

    void run() const override {
        polymorphism();
        table();
        smallOrders();
    }

private:
    using Base = BTree<BTreeAscTraits<TX>>;
    using Star = BStarTree<BTreeAscTraits<TX>>;

    SideBySideView m_view;

    static string bar(Size tenths, bool star) {
        Size filled = tenths * 20 / 1000;
        string full, empty;
        for (Size i{}; i < 20; ++i) (i < filled ? full : empty) += (i < filled ? "█" : "░");
        return (star ? styled(full).bright_green().str() : styled(full).bright_yellow().str()) + styled(empty).dim().str();
    }

    void polymorphism() const {
        Subtitle("La misma secuencia via unique_ptr<BTree<...>>");
        vector<unique_ptr<Base>> trees;
        trees.push_back(make_unique<Base>(5));
        trees.push_back(make_unique<Star>(5));
        vector<TX> keys(18);
        std::iota(keys.begin(), keys.end(), TX{1});
        std::mt19937 rng(2026);
        std::shuffle(keys.begin(), keys.end(), rng);
        for (auto& tree : trees) {
            for (TX k : keys) tree->insert(k * 5, k);
            for (TX k : {10, 35, 60, 85}) tree->remove(k);
        }
        m_view.compare(TreeView::project(*trees[0]), "BTree orden 5 (via BTree&)",
                       TreeView::project(*trees[1]), "BStarTree orden 5 (via BTree&)");
        const vector<string> names = {"BTree    ", "BStarTree"};
        for (Index i{}; i < trees.size(); ++i) {
            BTreeShape shape = btree_shape(*trees[i]);
            cout << "  " << names[i] << ": capacityFor(raiz) = " << trees[i]->capacityFor(true)
                 << " (despacho virtual), altura " << shape.height << ", paginas " << shape.pages
                 << ", " << trees[i]->size() << " claves\n";
        }
        cout << "\n";
    }

    void table() const {
        Subtitle("Ocupacion tras insertar 10 000 claves aleatorias (semilla 2026)");
        vector<TX> keys(10000);
        std::iota(keys.begin(), keys.end(), TX{1});
        std::mt19937 rng(2026);
        std::shuffle(keys.begin(), keys.end(), rng);
        cout << "  " << left << setw(7) << "Orden" << setw(11) << "Arbol" << right << setw(7) << "Altura"
             << setw(9) << "Paginas" << "  Ocupacion no raiz\n";
        for (Size order : {Size{7}, Size{10}, Size{20}}) {
            Base classic(order);
            Star star(order);
            for (TX k : keys) { classic.insert(k, 0); star.insert(k, 0); }
            auto row = [&](const string& kind, const BTreeShape& shape, Size tenths, bool isStar) {
                cout << "  " << left << setw(7) << order << setw(11) << kind << right << setw(7) << shape.height
                     << setw(9) << shape.pages << "  " << bar(tenths, isStar) << " " << btree_percent(tenths) << "\n";
            };
            BTreeShape c = btree_shape(classic), s = btree_shape(star);
            row("BTree", c, btree_occupancy_tenths(c, classic.capacityFor(false)), false);
            row("BStarTree", s, btree_occupancy_tenths(s, star.capacityFor(false)), true);
        }
        cout << "\n";
    }

    void smallOrders() const {
        string values;
        for (Size order : {Size{4}, Size{5}}) {
            Base classic(order);
            Star star(order);
            for (TX k = 1; k <= 40; ++k) { classic.insert(k, 0); star.insert(k, 0); }
            values += " m=" + std::to_string(order) + ": " + std::to_string(star.minKeysFor(*star.getRoot()->child(0)))
                    + " vs " + std::to_string(classic.minKeysFor(*classic.getRoot()->child(0))) + ";";
        }
        cout << "Con ordenes <= 5 el minimo 2/3 casi coincide con 1/2 (B* ⌊2(m-1)/3⌋ vs B ⌊(m-1)/2⌋):" << values
             << " con tan pocas claves por pagina la diferencia es de a lo sumo 1 clave (y m=3 no se admite en B*).\n";
    }
};

// --- ConcurrencyDemo ---

class ConcurrencyDemo final : public DemoSection {
protected:
    string_view title() const override { return "Concurrencia en BStarTree"; }

    void run() const override {
        using Star = BStarTree<BTreeAscTraits<TX>>;
        const TX perThread = 2000;
        Star tree(5);
        vector<thread> workers;
        for (TX t = 0; t < NThreads; ++t)
            workers.emplace_back([&tree, t, perThread] {
                for (TX v = t * perThread + 1; v <= (t + 1) * perThread; ++v) tree.insert(v, t);
            });
        for (auto& worker : workers) worker.join();

        TX expected = NThreads * perThread;
        TXX expectedSum = 0, actualSum = 0;
        for (TX v = 1; v <= expected; ++v) expectedSum += v;
        tree.call([](const auto& node, TXX& sum) { sum += node.getValue(); }, actualSum);
        string error = validateBTree(tree);
        cout << "DemoBTreeConcurrency: " << NThreads << " hilos, se esperaban " << expected
             << " claves, el BStarTree quedo con " << tree.size() << "; suma esperada = " << expectedSum
             << ", obtenida = " << actualSum << "; "
             << (error.empty() ? styled("").bright_green().str() : styled("invariantes ROTAS: " + error).bright_red().str())
             << "\n";
    }
};

} // namespace

void DemoBTree() { BTreeDemo{}.execute(); }

void DemoBStarTree() { BStarTreeDemo{}.execute(); }

void DemoBTreeComparison() { ComparisonDemo{}.execute(); }

void DemoBTreeConcurrency() { ConcurrencyDemo{}.execute(); }

void DemoGotoxy() {
    cout << "========================================\n";
    PrintColored(cout, "Demo Gotoxy (Coordenadas directas sin Canvas2D)", Color::Yellow);
    cout << "========================================\n\n";

    const TX box_x = 3;
    const TX box_y = 1;
    const TX box_w = 22;
    const TX box_h = 7;
    const TX word_x = 6;
    const TX word_y = 3;
    const std::string word = "hola";

    const TX total_h = box_y + box_h + 1;
    gotoxy_origin(cout, total_h);

    // Regla de coordenadas X (arriba) e Y (izquierda) para verificar visualmente
    for (TX x = box_x; x < box_x + box_w; ++x) {
        gotoxy(x, 0);
        cout << styled(x % 10).dim();
    }
    for (TX y = box_y; y < box_y + box_h; ++y) {
        gotoxy(0, y);
        cout << styled(y).dim();
    }

    // Dibujar bordes de la caja directamente con gotoxy
    gotoxy(box_x, box_y);
    cout << "┌";
    for (TX x = box_x + 1; x < box_x + box_w - 1; ++x) cout << "─";
    cout << "┐";

    for (TX y = box_y + 1; y < box_y + box_h - 1; ++y) {
        gotoxy(box_x, y);
        cout << "│";
        gotoxy(box_x + box_w - 1, y);
        cout << "│";
    }

    gotoxy(box_x, box_y + box_h - 1);
    cout << "└";
    for (TX x = box_x + 1; x < box_x + box_w - 1; ++x) cout << "─";
    cout << "┘";

    // Imprimir "hola" en una coordenada explícita (no centrada) dentro de la caja
    gotoxy(word_x, word_y);
    cout << styled(word).bright_green().bold();

    // Posicionar cursor debajo de la caja e informar la coordenada exacta
    gotoxy(0, total_h);
    cout << "Palabra '" << word << "' impresa con gotoxy(" << word_x << ", " << word_y << ")"
         << " dentro de la caja [" << box_x << ".." << (box_x + box_w - 1)
         << ", " << box_y << ".." << (box_y + box_h - 1) << "].\n\n";
}