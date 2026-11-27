/**
 * @file btree_page_test.cpp
 * @ingroup trees
 * @brief Pruebas unitarias de `BTreePage` con rasgos ascendentes y descendentes.
 *
 * Verifica: orden de claves, relación hijos = claves + 1, enlace al padre en
 * todas las primitivas e independencia del clonado profundo.
 * Retorna 0 si todas las verificaciones pasan; 1 en caso contrario.
 */

#include "../containers/trees/btree.h"
#include "../types.h"
#include <iostream>
#include <string>
#include <vector>

namespace {

Size g_checks   = 0;   /**< Verificaciones ejecutadas. */
Size g_failures = 0;   /**< Verificaciones fallidas. */

/**
 * @brief Registra una verificación e informa en consola si falla.
 * @param ok Resultado de la condición.
 * @param what Descripción de lo verificado.
 */
void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::cout << "  [FALLA] " << what << '\n';
    }
}

/**
 * @brief Extrae los valores de las claves de una página.
 * @tparam Page Tipo de página.
 * @param page Página a leer.
 * @return Vector con los valores en el orden almacenado.
 */
template <typename Page>
std::vector<typename Page::value_type> keysOf(const Page& page) {
    std::vector<typename Page::value_type> out;
    for (Index i{}; i < page.keyCount(); ++i) out.push_back(page.key(i).getValue());
    return out;
}

/**
 * @brief Construye una hoja con los valores dados insertados en orden según `Compare`.
 * @tparam Page Tipo de página.
 * @param capacity Capacidad de la hoja.
 * @param values Valores a insertar (en cualquier orden).
 * @return Hoja propietaria.
 */
template <typename Page>
typename Page::PageOwner makeLeaf(Size capacity, const std::vector<typename Page::value_type>& values) {
    auto leaf = std::make_unique<Page>(capacity);
    for (const auto& v : values) leaf->insertSorted(v, v * 10);
    return leaf;
}

/**
 * @brief Ejecuta la batería de pruebas para unos rasgos de orden.
 * @tparam Traits `BTreeAscTraits<TX>` o `BTreeDescTraits<TX>`.
 * @param name Nombre a mostrar.
 * @param expectedOrder Orden esperado tras insertar {50, 10, 40, 20, 30}.
 */
template <typename Traits>
void runSuite(const std::string& name, const std::vector<TX>& expectedOrder) {
    using Compare = typename Traits::Compare;
    using Page    = typename Traits::template PageFor<Compare>;
    std::cout << "== BTreePage " << name << " ==\n";

    // --- Orden de claves y búsqueda binaria
    auto root = makeLeaf<Page>(4, {50, 10, 40, 20, 30});
    check(keysOf(*root) == expectedOrder, name + ": claves ordenadas tras insertSorted");
    check(root->isOverflow() && root->isFull(), name + ": 5 claves con capacidad 4 es overflow");
    check(root->isLeaf() && root->isRoot(), name + ": pagina nueva es hoja y raiz");
    for (Index i{}; i < expectedOrder.size(); ++i) {
        auto pos = root->find(expectedOrder[i]);
        check(pos.has_value() && *pos == i, name + ": find ubica cada clave en su posicion");
        check(root->key(i).getRef() == expectedOrder[i] * 10, name + ": la Ref acompana a su clave");
    }
    check(!root->find(35).has_value(), name + ": find de clave ausente retorna nullopt");
    Index gap = root->lowerBound(35);
    check(gap == 3 || gap == 2, name + ": lowerBound de 35 cae entre 30 y 40");
    check(!Compare{}(root->key(gap).getValue(), 35) &&
          (gap == 0 || Compare{}(root->key(gap - 1).getValue(), 35)),
          name + ": lowerBound respeta Compare");

    // --- eraseKey / insertKey en posición
    auto removed = root->eraseKey(2);
    check(removed.getValue() == expectedOrder[2] && root->keyCount() == 4, name + ": eraseKey retorna la clave central");
    root->insertKey(2, removed);
    check(keysOf(*root) == expectedOrder, name + ": insertKey restaura el orden");
    root->eraseKey(root->keyCount() - 1);   // queda en capacidad: 4 claves
    check(!root->isOverflow() && root->isFull(), name + ": 4 claves = llena sin overflow");

    // --- Hijos = claves + 1 y enlace al padre
    TX tag{};
    for (Index i{}; i <= root->keyCount(); ++i) {
        auto childPtr = root->insertChild(i, makeLeaf<Page>(4, {tag++}));
        check(childPtr->parent() == root.get(), name + ": insertChild enlaza el padre");
    }
    check(root->childCount() == root->keyCount() + 1, name + ": hijos = claves + 1");
    check(!root->isLeaf(), name + ": con hijos deja de ser hoja");
    for (Index i{}; i < root->childCount(); ++i) {
        auto pos = root->child(i)->positionInParent();
        check(pos.has_value() && *pos == i, name + ": positionInParent coincide con el indice");
    }
    check(!root->positionInParent().has_value(), name + ": la raiz no tiene posicion en padre");
    check(root->isWellFormed(), name + ": pagina interna bien formada");

    // Nietos: el enlace se mantiene en varios niveles
    auto grandchild = root->child(1)->insertChild(0, makeLeaf<Page>(4, {7}));
    root->child(1)->insertChild(1, makeLeaf<Page>(4, {8}));
    check(grandchild->parent() == root->child(1), name + ": nieto enlazado a su padre");
    check(root->isWellFormed(), name + ": arbol de dos niveles bien formado");

    // removeChild desenlaza y transfiere propiedad
    auto detached = root->removeChild(0);
    check(detached->parent() == nullptr, name + ": removeChild desenlaza el padre");
    check(root->childCount() == root->keyCount(), name + ": tras extraer un hijo la forma es transitoria");
    check(!root->isWellFormed(), name + ": isWellFormed detecta hijos != claves + 1");
    root->insertChild(0, std::move(detached));
    check(root->child(0)->parent() == root.get() && root->isWellFormed(), name + ": reinsertar restaura la forma");

    // --- Clonado profundo e independiente
    auto copy = root->clone();
    check(copy->isRoot() && copy->isWellFormed(), name + ": el clon es raiz y esta bien formado");
    check(keysOf(*copy) == keysOf(*root), name + ": el clon tiene las mismas claves");
    check(copy->childCount() == root->childCount(), name + ": el clon tiene los mismos hijos");
    bool distinct = true, linked = true;
    for (Index i{}; i < copy->childCount(); ++i) {
        distinct = distinct && copy->child(i) != root->child(i);
        linked   = linked   && copy->child(i)->parent() == copy.get();
    }
    check(distinct, name + ": el clon no comparte paginas");
    check(linked, name + ": los hijos del clon apuntan al clon");
    check(copy->child(1)->childCount() == 2 && copy->child(1)->child(0)->parent() == copy->child(1),
          name + ": el clon copia nietos con su enlace");

    copy->key(0).ref() = -1;
    copy->eraseKey(1);
    copy->child(1)->removeChild(0);
    check(root->key(0).getRef() != -1, name + ": modificar Ref del clon no afecta al original");
    check(root->keyCount() == 4 && root->child(1)->childCount() == 2, name + ": modificar el clon no afecta al original");
    check(root->isWellFormed(), name + ": el original sigue bien formado");

    // --- Errores de rango
    bool thrown = false;
    try { root->key(root->keyCount()); } catch (const std::out_of_range&) { thrown = true; }
    check(thrown, name + ": key fuera de rango lanza out_of_range");
    thrown = false;
    try { root->insertChild(0, nullptr); } catch (const std::invalid_argument&) { thrown = true; }
    check(thrown, name + ": insertChild nulo lanza invalid_argument");
}

} // namespace

TX main() {
    runSuite<BTreeAscTraits<TX>>("Asc",  {10, 20, 30, 40, 50});
    runSuite<BTreeDescTraits<TX>>("Desc", {50, 40, 30, 20, 10});
    std::cout << "Verificaciones: " << g_checks << ", fallas: " << g_failures << '\n';
    if (g_failures == 0) return 0;
    return 1;
}
