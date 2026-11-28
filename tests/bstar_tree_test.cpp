/**
 * @file bstar_tree_test.cpp
 * @ingroup trees
 * @brief Pruebas específicas del árbol B*: orden mínimo, división de raíz 1→3, absorción de la raíz,
 *        polimorfismo a través de `BTree<Traits>&` y medición de ocupación frente al árbol B clásico.
 *
 * Las pruebas masivas, de ciclo de vida, de mixins y de concurrencia del B*
 * están en `btree_test.cpp`, `btree_mixins_test.cpp` y
 * `btree_concurrency_test.cpp` (parametrizadas por tipo de árbol).
 * Retorna 0 si todo pasa; 1 en caso contrario.
 */

#include "../containers/trees/btree.h"
#include "../containers/trees/bstar_tree.h"
#include "../types.h"
#include "../containers/trees/btree_validator.h"
#include "../utilities/btree_metrics.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
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

/** @brief Valida invariantes (vía la interfaz que reciba: tipo concreto o `BTree<Traits>&`). */
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
 * @brief Aplica la misma secuencia a cualquier árbol a través de la interfaz base `BTree<Traits>&`.
 * @return `true` si todas las inserciones y borrados tuvieron éxito y el árbol quedó válido tras cada operación.
 */
template <typename Traits>
bool feedThroughBase(BTree<Traits>& tree, const std::vector<TX>& inserts, const std::vector<TX>& removals) {
    bool ok = true;
    for (TX v : inserts)  ok = tree.insert(v, v) && validateBTree(tree).empty() && ok;
    for (TX v : removals) ok = tree.remove(v) && validateBTree(tree).empty() && ok;
    return ok;
}

/** @brief Orden mínimo, fórmulas de capacidad/mínimo y división de raíz 1→3 / absorción. */
template <typename Traits>
void runStructure(const std::string& name) {
    using Star = BStarTree<Traits>;
    const std::string tag = name + " estructura B*: ";

    bool thrown = false;
    try { Star bad(3); } catch (const std::invalid_argument&) { thrown = true; }
    check(thrown, tag + "orden 3 lanza invalid_argument (minimo 4)");
    thrown = false;
    try { Star ok(4); } catch (const std::invalid_argument&) { thrown = true; }
    check(!thrown, tag + "orden 4 es valido");

    for (Size m : std::vector<Size>{4, 5, 7, 10, 20}) {
        Star tree(m);
        tree.insert(1, 1);
        check(tree.capacityFor(true) == 2 * m - 1 && tree.capacityFor(false) == m - 1,
              tag + "capacidades R = 2m-1 y C = m-1 para m=" + std::to_string(m));
        // Página no raíz ficticia para consultar el mínimo: hija de una raíz temporal.
        typename Star::Page root(1);
        root.insertChild(0, std::make_unique<typename Star::Page>(m - 1));
        check(tree.minKeysFor(*root.child(0)) == 2 * (m - 1) / 3 && tree.minKeysFor(root) == Size{1},
              tag + "minimo L = 2(m-1)/3 para m=" + std::to_string(m));
    }

    // Raíz hoja hasta R = 7 claves (m = 4); la octava inserción la divide en 3 páginas con 2 separadores.
    Star tree(4);
    for (TX v = 1; v <= 7; ++v) tree.insert(v, v);
    check(tree.height() == Size{1} && tree.getRoot()->keyCount() == Size{7}, tag + "la raiz hoja admite R = 7 claves");
    tree.insert(8, 8);
    check(tree.height() == Size{2} && tree.getRoot()->keyCount() == Size{2} && tree.getRoot()->childCount() == Size{3},
          tag + "SplitRoot 1->3: raiz con 2 claves y 3 hijos");
    checkValid(tree, tag + "invariantes tras SplitRoot");

    // Crecer y luego borrar todo: la altura debe volver a bajar por absorción de la raíz.
    for (TX v : shuffled(400, 3)) tree.insert(v + 8, v);
    Size grown = tree.height();
    check(grown >= Size{3}, tag + "el arbol crece a 3+ niveles");
    Size previous = grown;
    bool monotone = true, valid = true;
    for (TX v : shuffled(408, 4)) {
        tree.remove(v);
        valid    = valid && validateBTree(tree).empty();
        monotone = monotone && tree.height() <= previous;
        previous = tree.height();
    }
    check(valid, tag + "invariantes tras cada borrado (absorciones incluidas)");
    check(monotone && tree.size() == Size{} && tree.height() == Size{}, tag + "la altura baja hasta 0 al vaciar");
}

/** @brief Polimorfismo: misma secuencia vía `BTree<Traits>&` sobre un `BTree` y un `BStarTree`. */
template <typename Traits>
void runPolymorphism(const std::string& name) {
    using Classic = BTree<Traits>;
    using Star    = BStarTree<Traits>;
    const std::string tag = name + " polimorfismo: ";
    const Size order = 7;

    Classic classic(order);
    Star    star(order);
    std::vector<BTree<Traits>*> trees{&classic, &star};   // ambos a través de la interfaz base

    const std::vector<TX> inserts  = shuffled(1500, 11);
    std::vector<TX>       removals = shuffled(1500, 12);
    removals.resize(500);
    bool fed = true;
    for (BTree<Traits>* tree : trees) fed = feedThroughBase(*tree, inserts, removals) && fed;
    check(fed, tag + "misma secuencia valida tras cada operacion en ambos (via BTree&)");

    BTree<Traits>& asBase = star;
    check(asBase.capacityFor(true) == 2 * order - 1 && classic.capacityFor(true) == order - 1,
          tag + "capacityFor despacha al gancho de cada tipo");
    check(classic.size() == star.size() && std::equal(classic.begin(), classic.end(), star.begin(), star.end(),
          [](const auto& a, const auto& b) { return a.getValue() == b.getValue() && a.getRef() == b.getRef(); }),
          tag + "mismo contenido y recorrido");

    BTreeShape classicShape = btree_shape(classic, star);   // mínimo de referencia: el 2/3 del B*
    BTreeShape starShape    = btree_shape(star, star);
    check(starShape.belowMin == Size{}, tag + "el B* no deja paginas no raiz bajo 2/3");
    check(classicShape.belowMin > Size{}, tag + "el clasico si tiene paginas no raiz bajo 2/3");
    check(starShape.pages < classicShape.pages, tag + "el B* usa menos paginas");

    // Destrucción polimórfica a través de la base.
    std::unique_ptr<BTree<Traits>> owned = std::make_unique<Star>(5);
    for (TX v = 1; v <= 200; ++v) owned->insert(v, v);
    check(owned->capacityFor(true) == Size{9} && checkValid(*owned, tag + "B* via unique_ptr<BTree> valido"),
          tag + "B* administrado por unique_ptr<BTree>");
    owned.reset();
}

/** @brief Medición: 10 000 claves aleatorias; altura, páginas y ocupación no raíz tras insertar. */
template <typename Traits>
void runMeasurement(const std::string& name) {
    using Classic = BTree<Traits>;
    using Star    = BStarTree<Traits>;
    const std::vector<TX> keys = shuffled(10000, 2026);

    std::cout << "\n  Medicion " << name << " (10 000 claves aleatorias, semilla 2026, tras insertar)\n";
    std::cout << "  " << std::left << std::setw(7) << "Orden" << std::setw(11) << "Arbol"
              << std::right << std::setw(7) << "Altura" << std::setw(9) << "Paginas" << std::setw(20) << "Ocupacion no raiz" << '\n';
    for (Size order : std::vector<Size>{7, 10, 20}) {
        Classic classic(order);
        Star    star(order);
        for (TX v : keys) { classic.insert(v, v); star.insert(v, v); }
        BTreeShape  c  = btree_shape(classic, classic);
        BTreeShape  s  = btree_shape(star, star);
        Size   oc = btree_occupancy_tenths(c, classic.capacityFor(false));
        Size   os = btree_occupancy_tenths(s, star.capacityFor(false));
        auto row = [&](const std::string& kind, const BTreeShape& shape, Size tenths) {
            std::cout << "  " << std::left << std::setw(7) << order << std::setw(11) << kind << std::right
                      << std::setw(7) << shape.height << std::setw(9) << shape.pages << std::setw(20) << btree_percent(tenths) << '\n';
        };
        row("BTree", c, oc);
        row("BStarTree", s, os);
        check(checkValid(classic, name + " medicion BTree") && checkValid(star, name + " medicion BStarTree"),
              name + " medicion: ambos arboles validos (orden " + std::to_string(order) + ")");
        check(os > oc, name + " medicion: B* con mayor ocupacion que B (orden " + std::to_string(order) + ")");
        check(os >= Size{800}, name + " medicion: B* con ocupacion >= 80% (orden " + std::to_string(order) + ")");
        check(s.pages < c.pages, name + " medicion: B* con menos paginas (orden " + std::to_string(order) + ")");
    }
}

} // namespace

TX main() {
    runStructure<BTreeAscTraits<TX>>("Asc");
    runStructure<BTreeDescTraits<TX>>("Desc");
    runPolymorphism<BTreeAscTraits<TX>>("Asc");
    runPolymorphism<BTreeDescTraits<TX>>("Desc");
    runMeasurement<BTreeAscTraits<TX>>("Asc");
    runMeasurement<BTreeDescTraits<TX>>("Desc");
    std::cout << "\nVerificaciones: " << g_checks << ", fallas: " << g_failures << '\n';
    if (g_failures == 0) return 0;
    return 1;
}
