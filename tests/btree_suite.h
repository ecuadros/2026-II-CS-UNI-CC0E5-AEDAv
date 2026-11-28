/**
 * @file btree_suite.h
 * @ingroup trees
 * @brief Suite parametrizada por familia de árbol (`BTree` / `BStarTree`): invariantes, ciclo de vida e iteradores.
 *
 * Para cada orden de rasgos (Asc/Desc) y varios órdenes de árbol (incluido el
 * mínimo admitido) se valida el árbol completo con `BTreeValidator`: tras
 * CADA operación en los órdenes pequeños y cada `stride` operaciones en los
 * mayores. La usan dos ejecutables (`btree_test.cpp` y
 * `bstar_btree_test.cpp`) para que `make test` los corra en paralelo.
 */

#ifndef __TESTS_BTREE_SUITE_H__
#define __TESTS_BTREE_SUITE_H__

#include "../containers/trees/btree.h"
#include "../containers/trees/bstar_tree.h"
#include "../types.h"
#include "../containers/trees/btree_validator.h"
#include <algorithm>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {

Size g_checks   = 0;   /**< Verificaciones ejecutadas. */
Size g_failures = 0;   /**< Verificaciones fallidas. */

constexpr TX Count = 2000;   /**< Claves 1..Count usadas en las pruebas masivas. */

/** @brief Registra una verificación e informa si falla. */
void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::cout << "  [FALLA] " << what << '\n';
    }
}

/** @brief Valida el árbol y registra el resultado con contexto. */
template <typename Tree>
bool checkValid(const Tree& tree, const std::string& what) {
    std::string error = validateBTree(tree);
    check(error.empty(), what + (error.empty() ? "" : " -> " + error));
    return error.empty();
}

/** @brief Claves 1..n barajadas con semilla fija. */
std::vector<TX> shuffled(TX n, std::mt19937::result_type seed) {
    std::vector<TX> values(n);
    std::iota(values.begin(), values.end(), TX{1});
    std::mt19937 rng(seed);
    std::shuffle(values.begin(), values.end(), rng);
    return values;
}

/**
 * @brief Verifica que los recorridos directo e inverso estén ordenados y tengan `size()` elementos.
 * @return `true` si ambos recorridos son correctos.
 */
template <typename Tree>
bool traversalsOk(const Tree& tree) {
    using Compare = typename Tree::Compare;
    Size forward{}, backward{};
    bool ordered = true;
    const typename Tree::Node* prev = nullptr;
    for (auto it = tree.begin(); it != tree.end(); ++it, ++forward) {
        if (prev && !Compare{}(prev->getValue(), (*it).getValue())) ordered = false;
        prev = &*it;
    }
    prev = nullptr;
    for (auto it = tree.rbegin(); it != tree.rend(); ++it, ++backward) {
        if (prev && !Compare{}((*it).getValue(), prev->getValue())) ordered = false;
        prev = &*it;
    }
    return ordered && forward == tree.size() && backward == tree.size();
}

/** @brief Pruebas masivas de inserción, borrado y mezcla para un orden dado. */
template <typename Tree>
void runOrder(const std::string& name, Size order) {
    const std::string tag = name + " orden " + std::to_string(order) + ": ";

    // Validar cada operación en órdenes pequeños y muestrear los mayores.
    const Size stride = order <= Size{5} ? Size{1} : Size{20};
    Size ops{};
    auto due = [&] { return ++ops % stride == Size{}; };

    // --- Inserción 1..Count en orden aleatorio
    Tree tree(order);
    bool allValid = true, allInserted = true;
    for (TX v : shuffled(Count, 2026)) {
        allInserted = tree.insert(v, v * 10) && allInserted;
        if (allValid && due()) allValid = checkValid(tree, tag + "invariantes tras insertar " + std::to_string(v));
    }
    checkValid(tree, tag + "invariantes al terminar las inserciones");
    check(allInserted && tree.size() == Size{2000}, tag + "se insertan 2000 claves");
    check(!tree.insert(1000, 0) && tree.size() == Size{2000}, tag + "duplicado rechazado sin cambiar size");
    bool allFound = true;
    for (TX v = 1; v <= Count; ++v) {
        const TX* found = tree.find(v);
        allFound = allFound && found && *found == v;
    }
    check(allFound, tag + "find encuentra todas las claves");
    check(!tree.find(0) && !tree.find(Count + 1), tag + "find de clave ausente retorna nullptr");
    check(traversalsOk(tree), tag + "recorridos directo e inverso ordenados con size() elementos");
    check(tree.height() > Size{1}, tag + "el arbol crecio en altura");

    // --- Borrado total en orden aleatorio
    Tree full(tree);   // copia para la prueba de mezcla
    allValid = true;
    bool allRemoved = true;
    for (TX v : shuffled(Count, 77)) {
        allRemoved = tree.remove(v) && allRemoved;
        if (allValid && due()) allValid = checkValid(tree, tag + "invariantes tras borrar " + std::to_string(v));
    }
    checkValid(tree, tag + "invariantes al terminar los borrados");
    check(allRemoved && tree.size() == Size{} && tree.height() == Size{}, tag + "borrar todo deja el arbol vacio");
    check(!tree.remove(5), tag + "remove en arbol vacio retorna false");
    check(tree.begin() == tree.end() && tree.rbegin() == tree.rend() && tree.empty(), tag + "iterador vacio tras borrar todo");
    check(tree.insert(42, 420) && checkValid(tree, tag + "reutilizable tras vaciarse"), tag + "inserta tras vaciarse");

    // --- Mezcla de inserciones y borrados contra un std::set de referencia
    std::set<TX> oracle;
    for (TX v = 1; v <= Count; ++v) oracle.insert(v);
    std::mt19937 rng(9);
    std::uniform_int_distribution<TX> pick(1, Count + 200);
    std::bernoulli_distribution doInsert(0.45);
    allValid = true;
    bool agree = true;
    for (TX step = 0; step < 3000; ++step) {
        TX v = pick(rng);
        if (doInsert(rng)) agree = (full.insert(v, v) == oracle.insert(v).second) && agree;
        else               agree = (full.remove(v) == (oracle.erase(v) == Size{1})) && agree;
        if (allValid && due()) allValid = checkValid(full, tag + "invariantes en mezcla, paso " + std::to_string(step));
    }
    checkValid(full, tag + "invariantes al terminar la mezcla");
    check(agree && full.size() == oracle.size(), tag + "mezcla coincide con std::set");
    check(traversalsOk(full), tag + "recorridos tras la mezcla");
}

/** @brief Copia, auto-asignación, movimiento e iteradores en casos borde. */
template <typename Tree>
void runLifecycle(const std::string& name) {
    const std::string tag = name + " ciclo de vida: ";

    // --- Orden inválido
    bool thrown = false;
    try { Tree bad(Tree::MinOrder - 1); } catch (const std::invalid_argument&) { thrown = true; }
    check(thrown, tag + "orden MinOrder - 1 lanza invalid_argument");
    thrown = false;
    try { Tree bad(0); } catch (const std::invalid_argument&) { thrown = true; }
    check(thrown, tag + "orden 0 lanza invalid_argument");
    check(Tree().order() == Tree::DefaultOrder, tag + "orden por defecto");

    // --- Iterador: vacío y un elemento
    Tree empty(Tree::MinOrder);
    check(empty.begin() == empty.end() && empty.rbegin() == empty.rend(), tag + "iterador en arbol vacio");
    check(empty.height() == Size{} && checkValid(empty, tag + "arbol vacio valido"), tag + "altura 0 en vacio");
    Tree single(Tree::MinOrder);
    single.insert(7, 70);
    auto it = single.begin();
    check(it != single.end() && (*it).getValue() == 7 && (*it).getRef() == 70, tag + "un elemento: begin apunta a el");
    ++it;
    check(it == single.end(), tag + "un elemento: ++begin == end");
    auto rit = single.rbegin();
    check((*rit).getValue() == 7 && ++rit == single.rend(), tag + "un elemento: recorrido inverso");
    single.remove(7);
    check(single.begin() == single.end(), tag + "un elemento: vacio tras borrarlo");

    // --- Copia independiente
    Tree original(4);
    for (TX v : shuffled(300, 5)) original.insert(v, v);
    Tree copy(original);
    check(checkValid(copy, tag + "copia valida"), tag + "la copia es valida");
    check(copy.size() == original.size() && copy.order() == original.order() && copy.height() == original.height(),
          tag + "la copia conserva size, orden y altura");
    check(copy.getRoot() != original.getRoot(), tag + "la copia no comparte raiz");
    bool samePairs = std::equal(copy.begin(), copy.end(), original.begin(), original.end(),
        [](const auto& a, const auto& b) { return a.getValue() == b.getValue() && a.getRef() == b.getRef(); });
    check(samePairs, tag + "la copia tiene los mismos pares (valor, ref)");
    for (TX v = 1; v <= 150; ++v) copy.remove(v);
    copy.insert(1000, 1);
    *copy.find(200) = 200;   // mismo valor, no rompe el orden
    check(original.size() == Size{300} && original.find(1) && !original.find(1000), tag + "modificar la copia no altera el original");
    check(checkValid(original, tag + "original valido tras modificar la copia"), tag + "original intacto");
    Tree assigned(Tree::MinOrder + 2);
    assigned.insert(-5, 0);
    assigned = original;
    check(assigned.size() == Size{300} && assigned.order() == Size{4} && !assigned.find(-5),
          tag + "asignacion por copia reemplaza contenido y orden");
    Tree& alias = assigned;
    assigned = alias;
    check(assigned.size() == Size{300} && checkValid(assigned, tag + "valido tras auto-asignacion"), tag + "auto-asignacion no altera");

    // --- Movimiento
    const Size before = original.size();
    Tree moved(std::move(original));
    check(moved.size() == before && checkValid(moved, tag + "destino del movimiento valido"), tag + "move ctor: destino completo");
    check(original.size() == Size{} && original.empty() && original.getRoot() == nullptr, tag + "move ctor: origen vacio");
    check(original.insert(1, 1) && original.size() == Size{1} && checkValid(original, tag + "origen reutilizado"),
          tag + "move ctor: origen reutilizable");
    Tree target(6);
    target.insert(5000, 0);   // clave ausente en `moved` (1..300)
    target = std::move(moved);
    check(target.size() == before && target.order() == Size{4} && !target.find(5000), tag + "move assign: destino reemplazado");
    check(moved.size() == Size{} && moved.begin() == moved.end(), tag + "move assign: origen vacio");
    check(checkValid(target, tag + "move assign valido"), tag + "move assign: invariantes");

    // --- Mixins: Printable, Readable y Applicable (rebuild)
    Tree small(Tree::MinOrder);
    small.insert(2, 20);
    small.insert(1, 10);
    std::ostringstream os;
    os << small;
    check(os.str() == (typename Tree::Compare{}(1, 2) ? "[(1,10),(2,20)]" : "[(2,20),(1,10)]"), tag + "Printable imprime en orden");
    std::istringstream is("[(5,50),(3,30),(4,40)]");
    is >> small;
    check(small.size() == Size{3} && small.find(4) && checkValid(small, tag + "Readable valido"), tag + "Readable reemplaza contenido");
    small.ApplyFunction([](typename Tree::Node& n) { n.value() = -n.value(); });
    check(small.find(-3) && !small.find(3) && traversalsOk(small) && checkValid(small, tag + "rebuild valido"),
          tag + "ApplyFunction + rebuild reordena");
}

/**
 * @brief Ejecuta la suite completa para una familia de árboles.
 * @tparam Family Plantilla de alias `BTree` o `BStarTree`.
 * @param family Nombre a mostrar.
 * @param orders Órdenes a probar (desde el mínimo admitido).
 * @return 0 si todo pasa; 1 en caso contrario.
 */
template <template <typename> typename Family>
TX runSuite(const std::string& family, const std::vector<Size>& orders) {
    for (Size order : orders) {
        runOrder<Family<BTreeAscTraits<TX>>>(family + " Asc", order);
        runOrder<Family<BTreeDescTraits<TX>>>(family + " Desc", order);
        std::cout << "== " << family << " orden " << order << " listo (Asc y Desc) ==\n";
    }
    runLifecycle<Family<BTreeAscTraits<TX>>>(family + " Asc");
    runLifecycle<Family<BTreeDescTraits<TX>>>(family + " Desc");
    std::cout << "Verificaciones: " << g_checks << ", fallas: " << g_failures << '\n';
    if (g_failures == 0) return 0;
    return 1;
}

} // namespace

#endif // __TESTS_BTREE_SUITE_H__
