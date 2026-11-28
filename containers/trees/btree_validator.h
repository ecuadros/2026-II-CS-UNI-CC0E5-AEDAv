/**
 * @file btree_validator.h
 * @ingroup trees
 * @brief Validador reutilizable de invariantes para cualquier árbol derivado de `BaseBTree`.
 */

#ifndef __BTREE_VALIDATOR_H__
#define __BTREE_VALIDATOR_H__
#include "../../types.h"
#include <optional>
#include <string>

/**
 * @class BTreeValidator
 * @ingroup trees
 * @brief Verifica orden, separadores, ocupación, profundidad, enlaces y tamaño.
 *
 * Consulta los límites de la política por `minKeysFor()` y `capacityFor()`.
 * @warning Proteja la validación completa si otros hilos modifican el árbol.
 * @tparam Tree Tipo de árbol B.
 */
template <typename Tree>
class BTreeValidator {
public:
    using Page    = typename Tree::Page;
    using Node    = typename Tree::Node;
    using Compare = typename Tree::Compare;

private:
    const Tree&          m_tree;
    std::string          m_error;
    Size                 m_counted{};
    std::optional<Size>  m_leafDepth;

    /**
     * @brief Registra la primera violación encontrada.
     * @param message Descripción de la violación.
     * @return Siempre `false` (para encadenar con `return fail(...)`).
     */
    bool fail(const std::string& message) {
        if (m_error.empty()) m_error = message;
        return false;
    }

    /**
     * @brief Orden estricto por `Compare` entre dos nodos.
     * @param a Primer nodo.
     * @param b Segundo nodo.
     * @return `true` si `a` precede a `b`.
     */
    bool precedes(const Node& a, const Node& b) const { return Compare{}(a.getValue(), b.getValue()); }

    /**
     * @brief Verifica recursivamente el subárbol `page`.
     * @param page Página actual.
     * @param parent Padre esperado.
     * @param depth Profundidad de `page` (raíz = 1).
     * @param low Cota inferior exclusiva (separador izquierdo) o `nullptr`.
     * @param high Cota superior exclusiva (separador derecho) o `nullptr`.
     * @return `true` si el subárbol cumple todas las invariantes.
     */
    bool visit(const Page* page, const Page* parent, Size depth, const Node* low, const Node* high) {
        bool root = parent == nullptr;
        if (page->parent() != parent)                    return fail("padre incorrecto");
        if (page->capacity() != m_tree.capacityFor(root)) return fail("capacidad distinta de capacityFor()");
        if (page->isOverflow())                          return fail("pagina en overflow");
        if (page->keyCount() == 0)                       return fail("pagina sin claves");
        if (!root && page->keyCount() < m_tree.minKeysFor(*page))
            return fail("pagina no raiz bajo el minimo");

        for (Index i = 1; i < page->keyCount(); ++i)
            if (!precedes(page->key(i - 1), page->key(i))) return fail("claves fuera de orden en la pagina");
        if (low  && !precedes(*low, page->key(0)))                     return fail("clave no supera al separador izquierdo");
        if (high && !precedes(page->key(page->keyCount() - 1), *high)) return fail("clave no precede al separador derecho");

        m_counted += page->keyCount();

        if (page->isLeaf()) {
            if (!m_leafDepth) m_leafDepth = depth;
            if (*m_leafDepth != depth) return fail("hojas a distinta profundidad");
            return true;
        }
        if (page->childCount() != page->keyCount() + 1) return fail("hijos != claves + 1");
        for (Index i{}; i < page->childCount(); ++i) {
            const Node* lo = i == 0 ? low : &page->key(i - 1);
            const Node* hi = i == page->keyCount() ? high : &page->key(i);
            if (!visit(page->child(i), page, depth + 1, lo, hi)) return false;
        }
        return true;
    }

public:
    /** @brief Crea un validador. @param tree Árbol a validar (debe vivir mientras se use el validador). */
    explicit BTreeValidator(const Tree& tree) : m_tree(tree) {}

    /**
     * @brief Ejecuta la validación completa.
     * @return `true` si todas las invariantes se cumplen; si no, `error()` describe la primera violación.
     */
    bool run() {
        m_error.clear();
        m_counted = 0;
        m_leafDepth.reset();
        const Page* root = m_tree.getRoot();
        if (!root) {
            if (m_tree.size() != 0 || m_tree.height() != 0) return fail("arbol sin raiz con size/height distintos de 0");
            return true;
        }
        if (!visit(root, nullptr, 1, nullptr, nullptr)) return false;
        if (m_counted != m_tree.size())   return fail("size() no coincide con las claves reales");
        if (*m_leafDepth != m_tree.height()) return fail("height() no coincide con la profundidad de las hojas");
        return true;
    }

    /** @brief Resultado de la última validación. @return Primera invariante violada (vacía si no hubo error). */
    const std::string& error() const { return m_error; }
};

/**
 * @brief Atajo: valida `tree` y retorna el mensaje de error (vacío si es válido).
 * @tparam Tree Tipo de árbol.
 * @param tree Árbol a validar.
 * @return Primera invariante violada, o cadena vacía si el árbol es válido.
 */
template <typename Tree>
std::string validateBTree(const Tree& tree) {
    BTreeValidator<Tree> validator(tree);
    validator.run();
    return validator.error();
}

#endif // __BTREE_VALIDATOR_H__
