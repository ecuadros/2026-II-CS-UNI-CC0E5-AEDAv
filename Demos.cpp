
#include <cmath>
#include <iostream>
#include <fstream> // ofstream para escribir en archivo
#include <string>
#include <thread>
#include <vector>
#include "foreach.h"
#include "containers/vector.h"
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

template <typename Node>
void PrintNode(Node &node, ostream &os) {
    os << node << " ";
}

bool HasEvenSquareRoot(const GeneralNode<TX> &node) {
    TX value = node.getValue();
    if (value < 0)
        return false;
    TX root = static_cast<TX>(lround(sqrt(value)));
    return root * root == value && root % 2 == 0;
}

template <typename Container>
void PrintAll(Container &container, ostream &os) {
    ::ApplyFunction(container, PrintNode<typename Container::Node>, os);
}

template <typename Container>
typename Container::value_type SumValues(Container &container) {
    typename Container::value_type total{};
    for (auto &node : container)
        total += node.getValue();
    return total;
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
    ::ApplyFunction(container.begin(), container.end(), PrintNode<Node>, cout);
    cout << "]" << endl;

    cout << "Backward traversal: [";
    ::ApplyFunction(container.rbegin(), container.rend(), PrintNode<Node>, cout);
    cout << "]" << endl;
}

template <typename Container>
void TestRead(const string &filename) {
    ifstream in(filename);
    Container loaded;
    if (in >> loaded)
        cout << "Container read from " << filename << ": " << loaded << endl;
    else
        cout << "Container could not be read from " << filename << endl;
}

template <typename Container>
void TestFirstThat(Container &container) {
    auto found = container.FirstThat(HasEvenSquareRoot);
    cout << "FirstThat with even square root: ";
    if (found != container.end())
        cout << *found << endl;
    else
        cout << "not found" << endl;
}

template <typename Container>
void TestApplyFunction(Container &container, const typename Container::value_type &x) {
    container.ApplyFunction(AddX<typename Container::value_type>, x);
    cout << "After ApplyFunction(AddX, " << x << "): " << container << endl;
}

template <typename Container>
void TestCall(Container &container) {
    cout << "Vector::call returning void: [";
    container.call(PrintAll<Container>, cout);
    cout << "]" << endl;
    cout << "Vector::call returning value: " << container.call(SumValues<Container>) << endl;
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
    TestRead<decltype(vec)>("vector.txt");
    TestFirstThat(vec);
    TestApplyFunction(vec, 10);
    TestFirstThat(vec);
    TestCall(vec);

    ofstream("vector_str.txt", ios::trunc).close();
    Vector<VectorAscTraits<string>> strVec;
    TestContainer(strVec, {{"Hello", 1}, {"World", 2}}, "vector_str.txt");
    TestTraversal(strVec);
    TestRead<decltype(strVec)>("vector_str.txt");
    TestApplyFunction(strVec, "!");
    TestCall(strVec);
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
