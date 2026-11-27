/**
 * @file btree_concurrency_test.cpp
 * @ingroup trees
 * @brief Prueba de concurrencia de `BTree`: inserciones, borrados, búsquedas, copias e impresión en paralelo.
 *
 * Hilos simultáneos:
 * - 8 insertores de rangos disjuntos `[1 + t*Block, (t+1)*Block]`;
 * - 2 borradores de una carga previa disjunta (`Preload`);
 * - 2 buscadores de claves que se están insertando y borrando;
 * - 1 copiador que copia el árbol, valida la copia (instantánea tomada bajo
 *   bloqueo) e imprime el árbol original con `operator<<`.
 *
 * Al final: `size()` exacto, recorrido ordenado y `BTreeValidator` sin errores.
 * Se compila con ASan+UBSan y, aparte, con TSan (`make test`).
 * Retorna 0 si todo pasa; 1 si no.
 */

#include "../containers/trees/btree.h"
#include "../containers/trees/bstar_tree.h"
#include "../types.h"
#include "btree_validator.h"
#include <atomic>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr TX Inserters = 8;        /**< Hilos insertores. */
constexpr TX Removers  = 2;        /**< Hilos borradores. */
constexpr TX Searchers = 2;        /**< Hilos buscadores. */
constexpr TX Block     = 1000;     /**< Claves por hilo insertor. */
constexpr TX Preload   = 100000;   /**< Base de las claves precargadas que se borran. */
constexpr TX Copies    = 20;       /**< Copias que realiza el hilo copiador. */

Size g_checks   = 0;   /**< Verificaciones ejecutadas (solo hilo principal). */
Size g_failures = 0;   /**< Verificaciones fallidas (solo hilo principal). */

/** @brief Registra una verificación e informa si falla. */
void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::cout << "  [FALLA] " << what << '\n';
    }
}

/** @brief `true` si el recorrido (con `call`, que bloquea) está estrictamente ordenado y tiene `size()` claves. */
template <typename Tree>
bool sortedTraversal(const Tree& tree) {
    std::vector<TX> values;
    tree.call([](const typename Tree::Node& n, std::vector<TX>& out) { out.push_back(n.getValue()); }, values);
    for (Index i = 1; i < values.size(); ++i)
        if (!(values[i - 1] < values[i])) return false;
    return values.size() == tree.size();
}

/**
 * @brief Ejecuta el escenario concurrente completo sobre un tipo de árbol.
 * @tparam Tree `BTree<...>` o `BStarTree<...>`.
 * @param name Nombre a mostrar en los mensajes.
 */
template <typename Tree>
void runConcurrency(const std::string& name) {
    std::cout << "== Concurrencia " << name << " ==\n";
    const std::string tag = name + ": ";
    Tree tree(5);
    for (TX v = 1; v <= Removers * Block; ++v) tree.insert(Preload + v, v);

    std::atomic<bool> go{false};
    std::atomic<Size> insertFailures{0}, removeFailures{0}, badCopies{0}, found{0};
    std::vector<std::thread> workers;

    auto waitStart = [&] { while (!go.load()) std::this_thread::yield(); };

    for (TX t = 0; t < Inserters; ++t)
        workers.emplace_back([&, t] {
            waitStart();
            for (TX v = 1 + t * Block; v <= (t + 1) * Block; ++v)
                if (!tree.insert(v, v)) ++insertFailures;
        });

    for (TX t = 0; t < Removers; ++t)
        workers.emplace_back([&, t] {
            waitStart();
            for (TX v = 1 + t * Block; v <= (t + 1) * Block; ++v)
                if (!tree.remove(Preload + v)) ++removeFailures;
        });

    for (TX t = 0; t < Searchers; ++t)
        workers.emplace_back([&, t] {
            waitStart();
            for (TX round = 0; round < 3; ++round)
                for (TX v = 1 + t; v <= Inserters * Block; v += Searchers)
                    if (tree.find(v) || tree.find(Preload + v)) ++found;
        });

    workers.emplace_back([&] {
        waitStart();
        for (TX i = 0; i < Copies; ++i) {
            Tree snapshot(tree);   // copia bajo scoped_lock: instantánea consistente
            if (!validateBTree(snapshot).empty() || !sortedTraversal(snapshot)) ++badCopies;
            std::ostringstream os;
            os << tree;            // Printable bloquea durante todo el recorrido
            if (os.str().empty() || os.str().front() != '[' || os.str().back() != ']') ++badCopies;
        }
    });

    go = true;
    for (std::thread& w : workers) w.join();

    const Size expected = Size{8000};
    check(insertFailures.load() == Size{} && removeFailures.load() == Size{}, tag + "todas las inserciones y borrados tuvieron exito");
    check(badCopies.load() == Size{}, tag + "cada copia concurrente es un arbol valido y la impresion es completa");
    check(found.load() > Size{}, tag + "los buscadores encontraron claves durante la ejecucion");
    check(tree.size() == expected, tag + "size() exacto tras la concurrencia (8000)");
    check(sortedTraversal(tree), tag + "recorrido final ordenado con size() claves");
    std::string error = validateBTree(tree);
    check(error.empty(), tag + "BTreeValidator sobre el arbol final" + (error.empty() ? "" : " -> " + error));
    bool allPresent = true, noneLeft = true;
    for (TX v = 1; v <= Inserters * Block; ++v) allPresent = allPresent && tree.find(v);
    for (TX v = 1; v <= Removers * Block; ++v)  noneLeft   = noneLeft && !tree.find(Preload + v);
    check(allPresent && noneLeft, tag + "estan todas las insertadas y ninguna de las borradas");
}

} // namespace

TX main() {
    runConcurrency<BTree<BTreeAscTraits<TX>>>("BTree");
    runConcurrency<BStarTree<BTreeAscTraits<TX>>>("BStarTree");
    std::cout << "Verificaciones: " << g_checks << ", fallas: " << g_failures << '\n';
    if (g_failures == 0) return 0;
    return 1;
}
