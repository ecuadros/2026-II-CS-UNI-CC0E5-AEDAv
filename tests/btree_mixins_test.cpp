/**
 * @file btree_mixins_test.cpp
 * @ingroup trees
 * @brief Pruebas de los mixins heredados por `BTree` y `BStarTree` (FirstThat, rcall, ApplyFunction) y de su persistencia.
 *
 * No hay implementaciones propias de estos mixins en `BaseBTree`: se verifica
 * que `Searchable`, `Callable`, `Reversible`, `Applicable`, `Printable` y
 * `Readable` funcionan sobre `BTreeIterator`. Retorna 0 si todo pasa; 1 si no.
 */

#include "../containers/trees/btree.h"
#include "../containers/trees/bstar_tree.h"
#include "../types.h"
#include "btree_validator.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

Size g_checks   = 0;   /**< Verificaciones ejecutadas. */
Size g_failures = 0;   /**< Verificaciones fallidas. */

/** @brief Registra una verificación e informa si falla. */
void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::cout << "  [FALLA] " << what << '\n';
    }
}

/** @brief Valida invariantes con `BTreeValidator` y registra el resultado. */
template <typename Tree>
void checkValid(const Tree& tree, const std::string& what) {
    std::string error = validateBTree(tree);
    check(error.empty(), what + (error.empty() ? "" : " -> " + error));
}

// Predicados y funciones con la misma forma que los de demos.cpp.

/** @brief Predicado variádico: `true` si el valor del nodo supera `threshold`. */
template <typename Node, typename T>
bool IsGreaterThan(Node& node, T threshold) { return node.getValue() > threshold; }

/** @brief Predicado variádico de dos argumentos: `true` si el valor está en `[low, high]`. */
template <typename Node, typename T>
bool IsBetween(Node& node, T low, T high) { return node.getValue() >= low && node.getValue() <= high; }

/** @brief Suma `x` al valor del nodo. */
template <typename T>
void AddX(GeneralNode<T>& node, T x) { node.value() += x; }

/** @brief Niega el valor del nodo (invierte el orden). */
template <typename T>
void Negate(GeneralNode<T>& node) { node.value() = -node.value(); }

/** @brief Divide entre 2 (provoca colisiones de claves). */
template <typename T>
void Halve(GeneralNode<T>& node) { node.value() /= 2; }

/** @brief Pares `(valor, ref)` en el orden del recorrido directo. */
template <typename Tree>
std::vector<std::pair<TX, Ref>> pairsOf(const Tree& tree) {
    std::vector<std::pair<TX, Ref>> out;
    tree.call([](const typename Tree::Node& n, std::vector<std::pair<TX, Ref>>& acc) {
        acc.emplace_back(n.getValue(), n.getRef());
    }, out);
    return out;
}

/** @brief `true` si el recorrido directo está estrictamente ordenado por `Compare`. */
template <typename Tree>
bool sortedByCompare(const Tree& tree) {
    using Compare = typename Tree::Compare;
    auto pairs = pairsOf(tree);
    for (Index i = 1; i < pairs.size(); ++i)
        if (!Compare{}(pairs[i - 1].first, pairs[i].first)) return false;
    return true;
}

/** @brief Árbol con claves 1..n y `Ref = 100 * clave`. */
template <typename Tree>
Tree makeTree(Size order, TX n) {
    Tree tree(order);
    for (TX v = 1; v <= n; ++v) tree.insert(v, Ref{100} * v);
    return tree;
}

/** @brief FirstThat, reversed().FirstThat y rcall. */
template <typename Tree>
void runSearch(const std::string& name) {
    using Node    = typename Tree::Node;
    using Compare = typename Tree::Compare;
    const bool asc = Compare{}(1, 2);
    const std::string tag = name + " busqueda: ";
    Tree tree = makeTree<Tree>(4, 50);

    // Predicado sin argumentos extra (lambda)
    Node& seven = tree.FirstThat([](Node& n) { return n.getValue() % 7 == 0; });
    check(seven.getValue() == (asc ? 7 : 49), tag + "FirstThat sin argumentos");
    check(seven.getRef() == Ref{100} * seven.getValue(), tag + "FirstThat retorna el nodo con su Ref");

    // Predicados con argumentos variádicos (estilo demos.cpp)
    Node& greater = tree.FirstThat(IsGreaterThan<Node, TX>, TX{10});
    check(greater.getValue() == (asc ? 11 : 50), tag + "FirstThat(IsGreaterThan, 10)");
    Node& between = tree.FirstThat(IsBetween<Node, TX>, TX{20}, TX{30});
    check(between.getValue() == (asc ? 20 : 30), tag + "FirstThat(IsBetween, 20, 30)");

    // El nodo retornado es el almacenado: cambiar su Ref se ve en el árbol
    greater.ref() = -1;
    check(tree.FirstThat(IsGreaterThan<Node, TX>, TX{10}).getRef() == -1, tag + "FirstThat retorna referencia al nodo real");

    // Sin coincidencias
    bool thrown = false;
    try { tree.FirstThat(IsGreaterThan<Node, TX>, TX{1000}); } catch (const std::runtime_error&) { thrown = true; }
    check(thrown, tag + "FirstThat sin coincidencias lanza runtime_error");
    thrown = false;
    Tree empty(Tree::MinOrder);
    try { empty.FirstThat(IsGreaterThan<Node, TX>, TX{0}); } catch (const std::runtime_error&) { thrown = true; }
    check(thrown, tag + "FirstThat en arbol vacio lanza runtime_error");

    // Vista invertida
    Node& lastGreater = tree.reversed().FirstThat(IsGreaterThan<Node, TX>, TX{10});
    check(lastGreater.getValue() == (asc ? 50 : 11), tag + "reversed().FirstThat");

    // rcall recorre exactamente al revés que call
    std::vector<TX> forward, backward;
    tree.call([](Node& n, std::vector<TX>& out) { out.push_back(n.getValue()); }, forward);
    tree.rcall([](Node& n, std::vector<TX>& out) { out.push_back(n.getValue()); }, backward);
    std::vector<TX> reversedForward(forward.rbegin(), forward.rend());
    check(forward.size() == Size{50} && backward == reversedForward, tag + "rcall es el inverso de call");
}

/** @brief ApplyFunction: suma (conserva orden), negación (rebuild reordena) y colisiones. */
template <typename Tree>
void runApply(const std::string& name) {
    const std::string tag = name + " ApplyFunction: ";

    // Suma: conserva el orden y las Ref
    Tree tree = makeTree<Tree>(Tree::MinOrder, 40);
    auto before = pairsOf(tree);
    tree.ApplyFunction(AddX<TX>, TX{5});
    auto after = pairsOf(tree);
    bool shifted = after.size() == before.size();
    for (Index i{}; shifted && i < after.size(); ++i)
        shifted = after[i].first == before[i].first + 5 && after[i].second == before[i].second;
    check(shifted, tag + "suma +5 conserva orden y Ref");
    check(tree.find(45) && !tree.find(1), tag + "suma +5 actualiza las claves buscables");
    checkValid(tree, tag + "invariantes tras suma");

    // Negación: invierte el orden; rebuild lo restaura
    tree.ApplyFunction(Negate<TX>);
    check(tree.size() == Size{40} && sortedByCompare(tree), tag + "negacion: rebuild restaura el orden");
    check(tree.find(-45) && tree.find(-6) && !tree.find(45), tag + "negacion: claves negadas buscables");
    bool refsKept = true;
    for (const auto& [value, ref] : pairsOf(tree)) refsKept = refsKept && ref == Ref{100} * (-value - 5);
    check(refsKept, tag + "negacion: cada Ref acompana a su clave");
    checkValid(tree, tag + "invariantes tras negacion");

    // Colisiones: x/2 vuelve equivalentes pares de claves; se conserva la primera del recorrido
    Tree halved = makeTree<Tree>(5, 20);
    std::map<TX, Ref> expected;   // primera aparición en el recorrido previo
    for (const auto& [value, ref] : pairsOf(halved)) expected.emplace(value / 2, ref);
    halved.ApplyFunction(Halve<TX>);
    check(halved.size() == expected.size() && halved.size() == Size{11}, tag + "x/2 descarta duplicados (20 -> 11 claves)");
    bool firstKept = true;
    for (const auto& [value, ref] : pairsOf(halved)) firstKept = firstKept && expected.at(value) == ref;
    check(firstKept, tag + "x/2 conserva la primera clave del recorrido con su Ref");
    check(sortedByCompare(halved), tag + "x/2 queda ordenado");
    checkValid(halved, tag + "invariantes tras colisiones");
}

/** @brief Persistencia con `<<` / `>>` a archivo, leyendo en un árbol de OTRO orden. */
template <typename Tree>
void runPersistence(const std::string& name, const std::filesystem::path& dir) {
    const std::string tag = name + " persistencia: ";

    auto roundTrip = [&](const Tree& source, Size targetOrder, const std::string& file, const std::string& what) {
        std::filesystem::path path = dir / file;
        {
            std::ofstream out(path, std::ios::trunc);
            out << source;
        }
        Tree target(targetOrder);
        target.insert(-999, 0);   // contenido previo que Readable debe reemplazar
        {
            std::ifstream in(path);
            in >> target;
        }
        check(target.order() == targetOrder && target.order() != source.order(), tag + what + ": destino conserva su propio orden");
        check(target.size() == source.size(), tag + what + ": mismo size");
        check(pairsOf(target) == pairsOf(source), tag + what + ": mismo recorrido (valor, ref)");
        check(!target.find(-999), tag + what + ": reemplaza el contenido previo");
        checkValid(target, tag + what + ": invariantes del arbol leido");
    };

    Tree empty(5);
    roundTrip(empty, Tree::MinOrder, name + "_vacio.txt", "arbol vacio");
    Tree tree = makeTree<Tree>(5, 300);
    roundTrip(tree, Tree::MinOrder, name + "_300_a_minimo.txt", "300 claves orden 5 -> MinOrder");
    roundTrip(tree, 16, name + "_300_a_orden16.txt", "300 claves orden 5 -> 16");
    tree.ApplyFunction(Negate<TX>);
    roundTrip(tree, 4, name + "_negadas.txt", "claves negativas");
}

/** @brief Ejecuta búsqueda, ApplyFunction y persistencia para un tipo de árbol. */
template <typename Tree>
void runAll(const std::string& name, const std::filesystem::path& dir) {
    runSearch<Tree>(name);
    runApply<Tree>(name);
    runPersistence<Tree>(name, dir);
}

} // namespace

TX main() {
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "aed_btree_mixins_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    runAll<BTree<BTreeAscTraits<TX>>>("BTree_Asc", dir);
    runAll<BTree<BTreeDescTraits<TX>>>("BTree_Desc", dir);
    runAll<BStarTree<BTreeAscTraits<TX>>>("BStarTree_Asc", dir);
    runAll<BStarTree<BTreeDescTraits<TX>>>("BStarTree_Desc", dir);

    std::filesystem::remove_all(dir);
    std::cout << "Verificaciones: " << g_checks << ", fallas: " << g_failures << '\n';
    if (g_failures == 0) return 0;
    return 1;
}
