
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

template <typename Container>
using Values = std::vector<std::pair<typename Container::value_type, Ref>>;

template <typename C>
concept Insertable = requires(C& c, const typename C::value_type& v, Ref r) { c.insert(v, r); };

template <typename C>
concept HeapLike = requires(C& c) {
    c.top();
    c.pop();
};

template <typename C>
concept TreeLike = requires(C& t, TX key, std::ostream& os) {
    t.InOrder();
    t.PreOrder();
    t.PostOrder();
    t.height();
    t.remove(key);
    t.print_tree(os, visual::Layout::Sideways);
};

template <typename C>
concept SixOrderTree = requires(const C& t) {
    t.InOrder();
    t.ReverseInOrder();
    t.PreOrder();
    t.ReversePreOrder();
    t.PostOrder();
    t.ReversePostOrder();
};

template <typename C>
concept Mutable = requires(C& c) { c.ApplyFunction([](auto&) {}); };

template <typename C>
concept Circular = visual::CircularView<C>;

template <typename C>
concept Copyable = std::is_copy_constructible_v<C> && std::is_copy_assignable_v<C>;

template <typename T> auto KeyOf(const T& value) {
    if constexpr (requires { value.key(); })
        return value.key();
    else
        return value;
}

void AddOne(GeneralNode<TX>& node) {
    node.value() += 1;
}

template <typename T> void AddX(GeneralNode<T>& node, T x) {
    node.value() += x;
}

void Square(GeneralNode<TX>& node) {
    node.value() *= node.value();
}

template <typename Node> void PrintNode(const Node& node, std::ostream& os) {
    os << node << " ";
}
const int NThreads = 5;

template <typename Value>
void PrintColor(const Value& value, std::string_view color = "1;36", std::ostream& os = std::cout) {
    os << visual::Painter(os)(color, visual::to_text(value)) << '\n';
}

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

template <typename Container> void SaveTo(const std::string& filename, const Container& container) {
    std::ofstream file(filename, std::ios::trunc);
    if (!file) throw std::runtime_error("No se pudo crear " + filename);
    file << container << '\n';
    if (!file) throw std::runtime_error("No se pudo escribir " + filename);
}

template <typename Container> void LoadFrom(const std::string& filename, Container& container) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("No se pudo abrir " + filename);
    if (!(file >> container)) throw std::runtime_error("No se pudo leer " + filename);
}

template <typename Container> void InsertAll(Container& container, const Values<Container>& values) {
    for (const auto& [value, ref] : values)
        container.insert(value, ref);
}

template <typename Iterator> struct Range {
    Iterator first, last;
    Iterator begin() const { return first; }
    Iterator end() const { return last; }
};

template <typename Container> auto ReverseRange(const Container& container) {
    return Range{container.rbegin(), container.rend()};
}

template <typename Body> void PrintBracketed(const std::string& label, Body body) {
    std::cout << label << ": [";
    body();
    std::cout << "]\n";
}

template <typename Traversal>
void PrintTraversal(const std::string& label, const Traversal& traversal) {
    PrintBracketed(label, [&] {
        for (auto&& node : traversal)
            std::cout << node << " ";
    });
}

template <typename Traversal> std::vector<Ref> CollectRefs(const Traversal& traversal) {
    std::vector<Ref> refs;
    for (auto&& node : traversal)
        refs.push_back(node.getRef());
    return refs;
}

template <typename Forward, typename Backward>
void CheckReverse(const std::string& name, const Forward& forward, const Backward& backward) {
    std::vector<Ref> refs = CollectRefs(backward);
    std::reverse(refs.begin(), refs.end());
    if (refs != CollectRefs(forward))
        throw std::runtime_error(name + ": el recorrido inverso no coincide");
}

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

template <typename Container>
void TestVisualize(Container& container, const std::string& name) {
    PrintColor("TestVisualize (" + name + ")");
    container.visualize(std::cout);
    if constexpr (visual::HasTreeView<Container> && !TreeLike<Container>) {
        std::cout << "Como arreglo:\n";
        container.print_linear(std::cout);
    }
}

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

template <typename Container> void TestStructure(const std::string& name) {
    Container container;
    TestStructure(container, name);
}

template <typename Container> void DemoRaceCondition(const std::string& name) {
    PrintColor("DemoRaceCondition (" + name + ")");
    const size_t N = 200000;
    Values<Container> values(N);
    for (size_t i = 0; i < N; ++i)
        values[i] = {static_cast<TX>(i), static_cast<Ref>(i)};

    std::cout << "DemoRaceCondition (" << NThreads << " hilos, push_back):\n";
    TestRaceCondition<Container>(name, values);
}


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

template <Mutable Container> void TestApplyFunction(Container& container) {
    container.ApplyFunction(AddOne);
    std::cout << "Tras AddOne: ";
    PrintColor(container);
    container.ApplyFunction(AddX<TX>, TX(10));
    std::cout << "Tras AddX(10): ";
    PrintColor(container);
}

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

template <TreeLike Tree> void CheckTree(const Tree& tree, const std::string& name) {
    std::vector<TX> values;
    for (auto&& node : tree.InOrder())
        values.push_back(KeyOf(node.getValue()));
    if (!std::is_sorted(values.begin(), values.end()) || values.size() != tree.size())
        throw std::runtime_error(name + ": el arbol perdio su orden");
}

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

template <typename C, typename Desc = void> void Demo(const std::string& name) {
    C container;
    Demo<Desc>(container, name);
}

template <typename T> std::string KeyText(const T& value) {
    if constexpr (requires { value.key(); })
        return visual::to_text(value.key());
    else
        return visual::to_text(value);
}

template <typename Tree>
class Unbalanced : public BinaryTree<BinaryTreeTraits<typename Tree::value_type, typename Tree::Compare,
                                                       BinaryTreeNode<typename Tree::value_type, void, typename Tree::ref_type>>> {
    using Parent = BinaryTree<BinaryTreeTraits<typename Tree::value_type, typename Tree::Compare,
                                               BinaryTreeNode<typename Tree::value_type, void, typename Tree::ref_type>>>;
    using typename Parent::Node;
    using typename Parent::NodePtr;

    mutable const Node* m_mark = nullptr;

    int balance(const Node* n) const {
        return static_cast<int>(this->height(n->m_pChild[1])) - static_cast<int>(this->height(n->m_pChild[0]));
    }

    NodePtr replacement(NodePtr node) const override {
        bool side = balance(node) >= 0;
        return extreme(node->m_pChild[side], !side);
    }

    void print_node(std::ostream& os, const Node& node) const override {
        bool marked = &node == m_mark;
        int bf = balance(&node);
        os << (marked ? "<" : "") << KeyText(node.getValue()) << '[' << (bf > 0 ? "+" : "") << bf << ']'
           << (marked ? ">" : "");
    }

public:
    explicit Unbalanced(const Tree& tree) {
        for (auto&& node : tree.PreOrder())
            this->insert(node.getValue(), node.getRef());
    }

    const Node* unbalanced() const {
        for (auto&& node : this->PostOrder())
            if (std::abs(balance(&node)) > 1) return &node;
        return nullptr;
    }

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

    void print(std::ostream& os, const Node* mark = nullptr) const {
        m_mark = mark;
        this->print_tree(os);
        m_mark = nullptr;
    }
};

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
            s.insert_unique(key, 0);
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

using MerkleStrings = MerkleOf<std::string>;
using MerkleHasher = StdMerkleHasher<std::string>;

std::string ShortHash(MerkleStrings::Digest digest) {
    std::ostringstream ss;
    ss << std::hex << std::setw(4) << std::setfill('0') << (digest & 0xffff);
    return ss.str();
}

void Check(bool ok, const std::string& name, const std::string& what) {
    if (!ok) throw std::runtime_error(name + ": " + what);
}

void CheckMerkle(bool ok, const std::string& what) { Check(ok, "MerkleTree", what); }

void PrintMerkleNode(const MerkleStrings::Node* node, const std::vector<std::string>& blocks, int depth) {
    if (!node) return;
    PrintMerkleNode(node->m_pChild[1], blocks, depth + 1);
    std::cout << std::string(depth * 8, ' ') << (node->is_leaf() ? blocks[node->getRef()] + ":" : "")
              << ShortHash(node->getValue()) << '\n';
    PrintMerkleNode(node->m_pChild[0], blocks, depth + 1);
}

void ShowMerkle(const MerkleStrings& tree, const std::vector<std::string>& blocks) {
    PrintMerkleNode(tree.tree_view().root(), blocks, 0);
    std::cout << "raiz = " << ShortHash(tree.root_hash()) << ", hojas = " << tree.size()
              << ", altura = " << tree.height() << "\n\n";
}

void TestMerkleProofs(const MerkleStrings& tree, const std::vector<std::string>& blocks) {
    PrintColor("TestMerkleProofs");
    auto root = tree.root_hash();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        auto proof = tree.proof(i);
        std::cout << "prueba(" << blocks[i] << ") = " << proof.size() << " hashes\n";
        CheckMerkle(MerkleStrings::verify(blocks[i], proof, root), "la prueba de " + blocks[i] + " no verifico");
        CheckMerkle(!MerkleStrings::verify(blocks[i] + "!", proof, root), "bloque alterado verifico");
        CheckMerkle(!MerkleStrings::verify(blocks[i], proof, root + 1), "raiz alterada verifico");
    }
    std::cout << "Todas las pruebas verifican; bloque o raiz alterados fallan\n";
}

void TestMerkleUpdate(MerkleStrings& tree, const std::vector<std::string>& blocks) {
    PrintColor("TestMerkleUpdate");
    auto before = tree.root_hash();
    MerkleStrings snapshot(tree);
    tree.update(2, "C2");
    std::cout << "update(2, \"C2\"): " << ShortHash(before) << " -> " << ShortHash(tree.root_hash()) << '\n';
    CheckMerkle(tree.root_hash() != before, "update no cambio la raiz");
    CheckMerkle(snapshot.root_hash() == before, "la copia no es independiente");
    auto edited = blocks;
    edited[2] = "C2";
    CheckMerkle(MerkleStrings(edited.begin(), edited.end()).root_hash() == tree.root_hash(),
                "update no coincide con reconstruir");
    tree.update(2, blocks[2]);
    CheckMerkle(tree.root_hash() == before, "restaurar el bloque no restauro la raiz");
    bool threw = false;
    try { tree.update(99, "x"); } catch (const std::out_of_range&) { threw = true; }
    CheckMerkle(threw, "update fuera de rango no lanzo");
    std::cout << "update coincide con reconstruir y es reversible\n";
}

void TestMerkleCopyAndClear() {
    PrintColor("TestCopyAndClear (MerkleTree)");
    MerkleStrings tree;
    bool threw = false;
    try { tree.root_hash(); } catch (const std::logic_error&) { threw = true; }
    CheckMerkle(tree.empty() && threw, "arbol vacio");
    tree = {"x", "y", "z"};
    MerkleStrings copy(tree);
    tree.clear();
    CheckMerkle(tree.empty() && copy.size() == 3, "clear afecto a la copia");
    tree = {"solo"};
    CheckMerkle(tree.height() == 1 && MerkleStrings::verify("solo", tree.proof(0), tree.root_hash()),
                "arbol de una hoja");
    std::cout << "clear, copia y arbol de una hoja OK\n";
}

void TestMerklePushBack() {
    PrintColor("TestMerklePushBack");
    MerkleStrings tree;
    std::vector<std::string> blocks;
    for (char c = 'A'; c <= 'I'; ++c) {
        blocks.push_back(std::string(1, c));
        tree.push_back(blocks.back());
        CheckMerkle(tree.size() == blocks.size(), "size tras push_back");
        CheckMerkle(tree.root_hash() == MerkleStrings(blocks.begin(), blocks.end()).root_hash(),
                    "push_back no coincide con reconstruir");
        for (std::size_t i = 0; i < blocks.size(); ++i)
            CheckMerkle(MerkleStrings::verify(blocks[i], tree.proof(i), tree.root_hash()), "prueba tras push_back");
    }
    tree.append("J");
    CheckMerkle(tree.size() == 10, "append");
    std::cout << "push_back/append 1..10 hojas: raiz y pruebas coinciden con reconstruir\n";
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
    Btc odd{a, b, c}, even{a, b, c, c};
    CheckMerkle(odd.root_hash() == even.root_hash(), "[a,b,c] debe dar la misma raiz que [a,b,c,c]");
    for (std::size_t i = 0; i < 3; ++i) {
        Sha256Digest leaf = i == 0 ? a : i == 1 ? b : c;
        CheckMerkle(Btc::verify(leaf, odd.proof(i), odd.root_hash()), "prueba con nodo duplicado");
    }
    odd.update(2, a);
    CheckMerkle(odd.root_hash() == Btc{a, b, a}.root_hash(), "update con nodo duplicado");
    odd.push_back(c);
    CheckMerkle(odd.root_hash() == Btc{a, b, a, c}.root_hash(), "push_back en Btc");
    CheckMerkle(Btc{a}.root_hash() == a, "una sola hoja: la raiz es el txid");
    std::cout << "raiz del bloque 170, duplicado del impar, pruebas, update y push_back OK\n";
}

void TestMerkleRace() {
    PrintColor("TestRaceCondition (MerkleTree)");
    MerkleStrings tree{"a", "b", "c", "d", "e", "f", "g", "h"};
    std::vector<std::thread> threads;
    for (int t = 0; t < NThreads; ++t)
        threads.emplace_back([&tree, t] {
            for (int i = 0; i < 2000; ++i) {
                tree.update(t, "v" + std::to_string(i));
                (void)tree.proof(t);
                (void)tree.root_hash();
            }
        });
    for (auto& thread : threads) thread.join();
    for (int t = 0; t < NThreads; ++t) tree.update(t, "v1999");
    MerkleStrings rebuilt{"v1999", "v1999", "v1999", "v1999", "v1999", "f", "g", "h"};
    CheckMerkle(tree.root_hash() == rebuilt.root_hash(), "estado inconsistente tras actualizar en paralelo");
    std::cout << NThreads << " hilos con update/proof/root_hash: raiz consistente\n";
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

    TestMerkleProofs(tree, blocks);
    TestMerkleUpdate(tree, blocks);
    TestMerklePushBack();
    TestMerkleCopyAndClear();
    TestMerkleRace();
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

void DemoHashTable() {
    PrintColor("DemoHashTable (operator[] y clave string)");
    auto check = [](bool ok, const std::string& what) { Check(ok, "HashTable", what); };
    HashTable<std::string, int> edades;
    auto show = [&](const std::string& label) { std::cout << label << ": " << edades << '\n'; };

    edades["ana"] = 20;
    edades["luis"] = 25;
    edades["eva"] = 30;
    show("insertar ana, luis, eva");
    edades["ana"] = 21;
    edades["eva"] += 1;
    show("ana = 21, eva += 1");
    check(edades.size() == 3 && edades["ana"] == 21 && edades["eva"] == 31, "operator[] fallo");
    check(edades["nadie"] == 0 && edades.size() == 4, "operator[] no creo la clave nueva");
    show("leer edades[\"nadie\"] la crea con 0");

    if (auto edad = edades.find("luis")) *edad += 5;
    check(edades.remove("nadie") && !edades.remove("nadie"), "remove fallo");
    check(!edades.find("pepe") && !edades.contains("pepe") && edades.contains("ana"), "find/contains fallo");
    show("luis += 5 con find, remove(\"nadie\")");

    std::stringstream stream("[(zoe,40),(beto,18),(ana,22)]");
    stream >> edades;
    show(">> de texto desordenado");
    check(stream && edades.size() == 3 && edades["zoe"] == 40, "operator>> fallo");
    stream.clear();
    stream.str("");
    stream << edades;
    check(stream.str() == "[(ana,22),(beto,18),(zoe,40)]", "operator<< fallo");
    TestInvalidRead(edades, "HashTable");
    edades.print_tree(std::cout);

    TestTree<HashTable<TX, Ref>::Tree>("HashTable (AVL interno)");
}
