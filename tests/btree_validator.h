/**
 * @file btree_validator.h
 * @ingroup trees
 * @brief Validador reutilizable de invariantes para cualquier árbol derivado de `BaseBTree`.
 *
 * Pide la capacidad y el mínimo de claves al propio árbol (`capacityFor()`,
 * `minKeysFor()`), de modo que sirve sin cambios para el árbol clásico y para
 * variantes que sobrescriban esos ganchos (p. ej. B*).
 */

#ifndef __BTREE_VALIDATOR_H__
#define __BTREE_VALIDATOR_H__
#include "../types.h"
#include <optional>
#include <string>

/**
 * @class BTreeValidator
 * @brief Recorre el árbol y verifica todas sus invariantes estructurales.
 *
 * Invariantes verificadas:
 * - orden estricto por `Compare` dentro de cada página;
 * - separadores: las claves del hijo `i` están entre las claves `i-1` e `i` del padre;
 * - hojas sin hijos; páginas internas con `keyCount() + 1` hijos;
 * - capacidad de cada página igual a `capacityFor(esRaiz)` y sin overflow;
 * - mínimo `minKeysFor(page)` en toda página salvo la raíz; raíz con al menos 1 clave;
 * - todas las hojas a la misma profundidad e igual a `height()`;
 * - enlace correcto al padre (y raíz sin padre);
 * - cantidad real de claves igual a `size()`.
 *
 * @tparam Tree Tipo de árbol (`BaseBTree<...>` o derivado).
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

    bool fail(const std::string& message) {
        if (m_error.empty()) m_error = message;
        return false;
    }

    bool precedes(const Node& a, const Node& b) const { return Compare{}(a.getValue(), b.getValue()); }

    /**
     * @brief Verifica recursivamente el subárbol `page`.
     * @param page Página actual.
     * @param parent Padre esperado.
     * @param depth Profundidad de `page` (raíz = 1).
     * @param low Cota inferior exclusiva (separador izquierdo) o `nullptr`.
     * @param high Cota superior exclusiva (separador derecho) o `nullptr`.
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
    /** @brief Crea un validador para `tree`. */
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

    /** @brief Descripción de la primera invariante violada (vacía si no hubo error). */
    const std::string& error() const { return m_error; }
};

/**
 * @brief Atajo: valida `tree` y retorna el mensaje de error (vacío si es válido).
 * @tparam Tree Tipo de árbol.
 */
template <typename Tree>
std::string validateBTree(const Tree& tree) {
    BTreeValidator<Tree> validator(tree);
    validator.run();
    return validator.error();
}

#endif // __BTREE_VALIDATOR_H__
