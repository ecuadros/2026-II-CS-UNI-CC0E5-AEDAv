/** @file
 * Pruebas genéricas que se reutilizan para todas las estructuras, y Demo<C>() que las combina.
 */
#pragma once

#include "helpers.h"

/** Llena el contenedor, lo guarda en un archivo y lo imprime. */
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

/** Prueba `call`, `rcall`, los iteradores y `FirstThat`. */
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

/** Dibuja el contenedor; los heaps se muestran además como arreglo. */
template <typename Container>
void TestVisualize(Container& container, const std::string& name) {
    PrintColor("TestVisualize (" + name + ")");
    container.visualize(std::cout);
    if constexpr (visual::HasTreeView<Container> && !TreeLike<Container>) {
        std::cout << "Como arreglo:\n";
        container.print_linear(std::cout);
    }
}

/** Prueba `push_back`, `insert`, copia, asignación, guardado, lectura y reutilización tras `clear`. */
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

/** Comprueba que una lectura con formato inválido falla y no modifica el contenedor. */
template <typename Container>
void TestInvalidRead(Container& container, const std::string& name, const std::string& text = "[(7,70),broken]") {
    std::ostringstream before;
    before << container;
    std::istringstream invalid(text);
    if (invalid >> container) throw std::runtime_error(name + ": lectura invalida aceptada");
    std::ostringstream after;
    after << container;
    if (before.str() != after.str())
        throw std::runtime_error(name + ": la lectura invalida cambio los datos");
}

/** Comprueba `clear` repetido, la copia, la autoasignación y la reutilización del contenedor. */
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

/** Inserta @p values desde varios hilos y comprueba que no se pierde ningún elemento. */
template <typename Container>
void TestRaceCondition(const std::string& name, const Values<Container>& values) {
    PrintColor("TestRaceCondition (" + name + ")");
    Container container;

    size_t n = values.size();
    RunThreads(NThreads, [&](Int t) {
        for (size_t i = t; i < n; i += NThreads)
            container.push_back(values[i].first, values[i].second);
    });

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

/** Ejecuta todas las pruebas genéricas sobre el contenedor. */
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

/** Ejecuta las pruebas genéricas sobre un contenedor recién creado. */
template <typename Container> void TestStructure(const std::string& name) {
    Container container;
    TestStructure(container, name);
}

/** Prueba de concurrencia con 200000 inserciones repartidas entre los hilos. */
template <typename Container> void DemoRaceCondition(const std::string& name) {
    PrintColor("DemoRaceCondition (" + name + ")");
    const size_t N = 200000;
    Values<Container> values(N);
    for (size_t i = 0; i < N; ++i)
        values[i] = {static_cast<TX>(i), static_cast<Ref>(i)};

    std::cout << "DemoRaceCondition (" << NThreads << " hilos, push_back):\n";
    TestRaceCondition<Container>(name, values);
}


/** Imprime el ciclo de una lista circular, en ambos sentidos si es doble. */
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

/** Prueba `ApplyFunction` con AddOne y AddX. */
template <Mutable Container> void TestApplyFunction(Container& container) {
    container.ApplyFunction(AddOne);
    std::cout << "Tras AddOne: ";
    PrintColor(container);
    container.ApplyFunction(AddX<TX>, TX(10));
    std::cout << "Tras AddX(10): ";
    PrintColor(container);
}

/** Prueba el contenedor con el comparador descendente. */
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

/**
 * Comprueba que el inorden esté ordenado y que el tamaño coincida.
 * @throws std::runtime_error si el árbol perdió su orden.
 */
template <TreeLike Tree> void CheckTree(const Tree& tree, const std::string& name) {
    std::vector<TX> values;
    for (auto&& node : tree.InOrder())
        values.push_back(KeyOf(node.getValue()));
    if (!std::is_sorted(values.begin(), values.end()) || values.size() != tree.size())
        throw std::runtime_error(name + ": el arbol perdio su orden");
}

/** Imprime los seis recorridos y comprueba que cada inverso coincide con su directo. */
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

/** Prueba `remove` con una hoja, un nodo con un hijo, la raíz y un valor inexistente, y la persistencia posterior. */
template <TreeLike Tree> void TestTreeRemove(Tree& tree, const std::string& name) {
    PrintColor("TestTreeRemove (" + name + ")");
    for (TX value : {20, 60, 50, 99}) {
        Bool removed = tree.remove(value);
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

/** Inserta 1..15 en orden y muestra la altura y el dibujo del árbol. */
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

/** Ejecuta las pruebas específicas de árboles: dibujo, recorridos, eliminación y altura. */
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

/**
 * Ejecuta sobre @p container todas las pruebas que su tipo admite.
 * @tparam Desc Variante con orden descendente del contenedor, o `void` para omitir esa prueba.
 * @tparam C Tipo del contenedor.
 */
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

/** Crea un contenedor de tipo @p C y ejecuta sus pruebas. */
template <typename C, typename Desc = void> void Demo(const std::string& name) {
    C container;
    Demo<Desc>(container, name);
}
