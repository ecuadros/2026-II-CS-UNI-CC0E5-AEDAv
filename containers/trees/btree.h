/**
 * @file btree.h
 * @ingroup trees
 * @brief Página del árbol B (`BTreePage`) y rasgos de orden (`BTreeTraits`, `BTreeAscTraits`, `BTreeDescTraits`).
 *
 * La página es la unidad estructural del árbol B: almacena claves ordenadas
 * como `GeneralNode<T>` (valor + `Ref`) dentro de un `BaseVector`, posee a sus
 * hijos mediante `std::unique_ptr` y mantiene un puntero no propietario a su
 * padre. Todas sus primitivas de modificación conservan el enlace al padre.
 *
 * La página NO decide políticas de ocupación mínima, redistribución ni
 * división: esas decisiones corresponden al árbol.
 */

#ifndef __BTREE_H__
#define __BTREE_H__
#include "../core/general_node.h"
#include "../core/general_container.h"
#include "../vector.h"
#include "../../types.h"
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

/**
 * @struct BTreeKeyTraits
 * @brief Rasgos del `BaseVector` que guarda las claves de una página.
 *
 * Combina el orden (`DefaultTraits`) con los nodos e iteradores de vector,
 * sin redefinir `Compare`.
 *
 * @tparam T Tipo de la clave.
 * @tparam Compare Comparador estricto de claves.
 */
template <typename T, typename Compare>
struct BTreeKeyTraits : public DefaultTraits<T, Compare>, public VectorTraits<T> {};

/**
 * @class BTreePage
 * @brief Página (nodo) de un árbol B con claves ordenadas, hijos propios y enlace al padre.
 *
 * Invariantes que garantizan las primitivas:
 * - Las claves quedan ordenadas según `Compare` si se insertan en la posición
 *   que retorna `lowerBound()`.
 * - Todo hijo insertado tiene `parent() == this`; todo hijo extraído queda con
 *   `parent() == nullptr`.
 * - En una página interna bien formada `childCount() == keyCount() + 1`
 *   (se verifica con `isWellFormed()`, no se impone en cada primitiva para
 *   permitir estados intermedios durante divisiones y fusiones).
 *
 * @tparam T Tipo de la clave almacenada.
 * @tparam Compare Comparador estricto (`std::less<T>`, `std::greater<T>`, ...).
 */
template <typename T, typename Compare>
class BTreePage {
public:
    using value_type = T;
    using MySelf     = BTreePage<T, Compare>;
    using KeyTraits  = BTreeKeyTraits<T, Compare>;
    using Keys       = BaseVector<KeyTraits>;
    using Node       = typename Keys::Node;      /**< `GeneralNode<T>`: valor + `Ref`. */
    using PagePtr    = MySelf*;                  /**< Puntero no propietario a página. */
    using PageOwner  = std::unique_ptr<MySelf>;  /**< Puntero propietario a página. */
    using Children   = std::vector<PageOwner>;

private:
    Keys     m_keys;               /**< Claves ordenadas de la página. */
    Children m_children;           /**< Hijos propios (vacío si la página es hoja). */
    PagePtr  m_parent = nullptr;   /**< Padre no propietario (`nullptr` en la raíz). */
    Size     m_capacity;           /**< Máximo de claves permitido en estado estable. */

    /** @brief Lanza `std::out_of_range` si `pos >= limit`. */
    static void checkRange(Index pos, Size limit, const char* what) {
        if (pos >= limit) throw std::out_of_range(what);
    }

public:
    /**
     * @brief Construye una página vacía (hoja) con la capacidad indicada.
     * @param capacity Número máximo de claves en estado estable.
     * @throws std::invalid_argument Si `capacity` es 0.
     */
    explicit BTreePage(Size capacity) : m_capacity(capacity) {
        if (capacity == 0) throw std::invalid_argument("La capacidad de la pagina debe ser positiva");
        m_keys.reserve(capacity + 1);
    }

    virtual ~BTreePage() = default;

    // La propiedad es única: las páginas se duplican con clone() y se mueven como PageOwner.
    BTreePage(const BTreePage&)            = delete;
    BTreePage& operator=(const BTreePage&) = delete;
    BTreePage(BTreePage&&)                 = delete;
    BTreePage& operator=(BTreePage&&)      = delete;

    // ---------------------------------------------------------------- consultas

    /** @brief Capacidad máxima de claves en estado estable. */
    Size capacity()   const { return m_capacity; }
    /** @brief Cantidad de claves almacenadas. */
    Size keyCount()   const { return m_keys.size(); }
    /** @brief Cantidad de hijos. */
    Size childCount() const { return m_children.size(); }
    /** @brief `true` si la página no tiene hijos. */
    bool isLeaf()     const { return m_children.empty(); }
    /** @brief `true` si la página no tiene padre. */
    bool isRoot()     const { return m_parent == nullptr; }
    /** @brief `true` si la página alcanzó su capacidad. */
    bool isFull()     const { return keyCount() >= m_capacity; }
    /** @brief `true` si la página excede su capacidad (estado transitorio de inserción). */
    bool isOverflow() const { return keyCount() > m_capacity; }

    /** @brief Padre no propietario (`nullptr` en la raíz). */
    PagePtr parent() const { return m_parent; }

    /**
     * @brief Nodo (valor + `Ref`) en la posición `pos`.
     * @param pos Posición de la clave.
     * @return Referencia constante al nodo.
     * @throws std::out_of_range Si `pos >= keyCount()`.
     */
    const Node& key(Index pos) const {
        checkRange(pos, keyCount(), "Posicion de clave fuera de rango");
        return m_keys[pos];
    }

    /**
     * @brief Nodo mutable en la posición `pos` (para actualizar `Ref` o reemplazar separadores).
     * @param pos Posición de la clave.
     * @return Referencia mutable al nodo.
     * @throws std::out_of_range Si `pos >= keyCount()`.
     */
    Node& key(Index pos) {
        checkRange(pos, keyCount(), "Posicion de clave fuera de rango");
        return m_keys[pos];
    }

    /**
     * @brief Hijo no propietario en la posición `pos`.
     * @param pos Posición del hijo.
     * @return Puntero a la página hija.
     * @throws std::out_of_range Si `pos >= childCount()`.
     */
    PagePtr child(Index pos) const {
        checkRange(pos, childCount(), "Posicion de hijo fuera de rango");
        return m_children[pos].get();
    }

    // ---------------------------------------------------------------- búsqueda

    /**
     * @brief Búsqueda binaria: primera posición cuya clave no precede a `value` según `Compare`.
     *
     * En una página interna, la posición retornada es también el índice del
     * hijo por el que debe descenderse cuando la clave no está en la página.
     *
     * @param value Valor buscado.
     * @return Posición en `[0, keyCount()]`.
     */
    Index lowerBound(const value_type& value) const {
        Index first{};
        Index last = keyCount();
        while (first < last) {
            Index mid = first + (last - first) / 2;
            if (Compare{}(m_keys[mid].getValue(), value)) first = mid + 1;
            else                                          last  = mid;
        }
        return first;
    }

    /**
     * @brief Indica si la clave en `pos` es equivalente a `value` (ni precede ni sucede).
     * @param pos Posición a comparar (típicamente el resultado de `lowerBound()`).
     * @param value Valor de referencia.
     * @return `true` si `pos < keyCount()` y la clave es equivalente.
     */
    bool matchesAt(Index pos, const value_type& value) const {
        return pos < keyCount() && !Compare{}(value, m_keys[pos].getValue())
                                && !Compare{}(m_keys[pos].getValue(), value);
    }

    /**
     * @brief Busca una clave equivalente a `value` solo en esta página.
     * @param value Valor buscado.
     * @return Posición de la clave o `std::nullopt` si no está.
     */
    std::optional<Index> find(const value_type& value) const {
        Index pos = lowerBound(value);
        if (matchesAt(pos, value)) return pos;
        return std::nullopt;
    }

    // ---------------------------------------------------------------- claves

    /**
     * @brief Inserta un nodo en la posición `pos`, desplazando a la derecha los siguientes.
     * @param pos Posición destino en `[0, keyCount()]`.
     * @param node Nodo (valor + `Ref`) a insertar.
     * @throws std::out_of_range Si `pos > keyCount()`.
     */
    void insertKey(Index pos, const Node& node) {
        checkRange(pos, keyCount() + 1, "Posicion de insercion de clave fuera de rango");
        m_keys.push_back(node.getValue(), node.getRef());
        for (Index i = keyCount() - 1; i > pos; --i)
            m_keys[i] = m_keys[i - 1];
        m_keys[pos] = node;
    }

    /** @brief Inserta `(value, ref)` en la posición `pos`. @see insertKey(Index, const Node&) */
    void insertKey(Index pos, const value_type& value, Ref ref) { insertKey(pos, Node(value, ref)); }

    /**
     * @brief Inserta `(value, ref)` en su posición ordenada.
     * @param value Valor a insertar.
     * @param ref Referencia asociada.
     * @return Posición donde quedó la clave.
     */
    Index insertSorted(const value_type& value, Ref ref) {
        Index pos = lowerBound(value);
        insertKey(pos, value, ref);
        return pos;
    }

    /**
     * @brief Elimina la clave en `pos`, desplazando a la izquierda las siguientes.
     * @param pos Posición de la clave.
     * @return El nodo eliminado.
     * @throws std::out_of_range Si `pos >= keyCount()`.
     */
    Node eraseKey(Index pos) {
        checkRange(pos, keyCount(), "Posicion de eliminacion de clave fuera de rango");
        Node removed = m_keys[pos];
        for (Index i = pos + 1; i < keyCount(); ++i)
            m_keys[i - 1] = m_keys[i];
        m_keys.pop_back();
        return removed;
    }

    // ---------------------------------------------------------------- hijos

    /**
     * @brief Inserta un hijo en la posición `pos` y lo enlaza a esta página como padre.
     * @param pos Posición destino en `[0, childCount()]`.
     * @param page Página hija (se transfiere su propiedad).
     * @return Puntero no propietario al hijo insertado.
     * @throws std::out_of_range Si `pos > childCount()`.
     * @throws std::invalid_argument Si `page` es nulo.
     */
    PagePtr insertChild(Index pos, PageOwner page) {
        checkRange(pos, childCount() + 1, "Posicion de insercion de hijo fuera de rango");
        if (!page) throw std::invalid_argument("No se puede insertar un hijo nulo");
        page->m_parent = this;
        PagePtr raw = page.get();
        m_children.insert(m_children.begin() + pos, std::move(page));
        return raw;
    }

    /**
     * @brief Extrae el hijo en `pos`, transfiriendo su propiedad y desenlazando su padre.
     * @param pos Posición del hijo.
     * @return La página extraída con `parent() == nullptr`.
     * @throws std::out_of_range Si `pos >= childCount()`.
     */
    PageOwner removeChild(Index pos) {
        checkRange(pos, childCount(), "Posicion de eliminacion de hijo fuera de rango");
        PageOwner page = std::move(m_children[pos]);
        m_children.erase(m_children.begin() + pos);
        page->m_parent = nullptr;
        return page;
    }

    /**
     * @brief Posición de `page` entre los hijos de esta página.
     * @param page Página buscada.
     * @return Su posición o `std::nullopt` si no es hija directa.
     */
    std::optional<Index> indexOfChild(const MySelf* page) const {
        for (Index i{}; i < childCount(); ++i)
            if (m_children[i].get() == page) return i;
        return std::nullopt;
    }

    /**
     * @brief Posición de esta página entre los hijos de su padre.
     * @return La posición o `std::nullopt` si la página es raíz.
     */
    std::optional<Index> positionInParent() const {
        if (!m_parent) return std::nullopt;
        return m_parent->indexOfChild(this);
    }

    // ---------------------------------------------------------------- copia y validación

    /**
     * @brief Copia profunda del subárbol con raíz en esta página.
     *
     * La copia es independiente (no comparte páginas) y su raíz no tiene padre.
     * @return Página raíz de la copia.
     */
    PageOwner clone() const {
        auto copy = std::make_unique<MySelf>(m_capacity);
        copy->m_keys = m_keys;
        for (const PageOwner& c : m_children)
            copy->insertChild(copy->childCount(), c->clone());
        return copy;
    }

    /**
     * @brief Verifica recursivamente la forma del subárbol.
     *
     * Comprueba: claves en orden no decreciente según `Compare`, hojas sin
     * hijos o internas con `keyCount() + 1` hijos, y enlace correcto al padre
     * en cada hijo. No verifica ocupación mínima (es política del árbol).
     * @return `true` si todas las condiciones se cumplen.
     */
    bool isWellFormed() const {
        for (Index i = 1; i < keyCount(); ++i)
            if (Compare{}(m_keys[i].getValue(), m_keys[i - 1].getValue())) return false;
        if (isLeaf()) return true;
        if (childCount() != keyCount() + 1) return false;
        for (const PageOwner& c : m_children)
            if (c->m_parent != this || !c->isWellFormed()) return false;
        return true;
    }
};

/**
 * @struct BTreeTraits
 * @brief Rasgos estructurales del árbol B independientes del orden.
 *
 * El tipo de página depende del comparador, que aportan `AscendingTraits` o
 * `DescendingTraits`; por eso se expone como plantilla de alias.
 * @tparam T Tipo de la clave.
 */
template <typename T>
struct BTreeTraits {
    using Node = GeneralNode<T>;   /**< Nodo de clave (valor + `Ref`). */
    /** @brief Página del árbol para el comparador `C`. */
    template <typename C>
    using PageFor = BTreePage<T, C>;
};

/** @brief Rasgos de árbol B con orden ascendente (`std::less<T>`). */
template <typename T>
struct BTreeAscTraits : public AscendingTraits<T>, public BTreeTraits<T> {};

/** @brief Rasgos de árbol B con orden descendente (`std::greater<T>`). */
template <typename T>
struct BTreeDescTraits : public DescendingTraits<T>, public BTreeTraits<T> {};

#endif // __BTREE_H__
