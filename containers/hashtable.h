#ifndef __HASHTABLE_H__
#define __HASHTABLE_H__

#include "avl.h"
#include "traits.h"
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <type_traits>
#include <utility>

/**
 * Tabla clave-valor con interfaz de mapa. Guarda las entradas en un AVL ordenado por clave, por lo que buscar, insertar y eliminar cuestan O(log n) y el recorrido sale ordenado.
 * @tparam Key Tipo de la clave.
 * @tparam Value Tipo del valor asociado.
 * @tparam Comparator Criterio de orden de las claves.
 */
template <typename Key, typename Value, typename Comparator = std::less<Key>> class HashTable {
public:
    using Tree = AVLTree<AVLTraits<Key, Comparator, Value>>;
    using key_type = Key;
    using mapped_type = Value;
    using value_type = std::pair<const Key, Value>;
    using ref_type = typename Tree::ref_type;
    using Compare = typename Tree::Compare;

private:
    /** Árbol que guarda cada clave con su valor como referencia. */
    Tree m_tree;

    /**
     * Adapta un iterador del árbol para que al desreferenciarlo dé un par (clave, valor).
     * @tparam TreeIterator Iterador del árbol que se envuelve.
     * @tparam V `Value` o `const Value`, según si permite modificar el valor.
     */
    template <typename TreeIterator, typename V> class EntryIterator {
        template <typename, typename> friend class EntryIterator;
        TreeIterator m_it;

    public:
        using reference = std::pair<const Key&, V&>;

        /** Objeto temporal que permite escribir `it->first` e `it->second`. */
        struct Arrow {
            reference entry;
            /** Devuelve la entrada guardada. */
            reference* operator->() { return &entry; }
        };

        /** Envuelve el iterador del árbol @p it. */
        EntryIterator(TreeIterator it) : m_it(it) {}

        /** Convierte un iterador modificable en uno constante. */
        template <typename U>
            requires std::same_as<const U, V>
        EntryIterator(const EntryIterator<TreeIterator, U>& other) : m_it(other.m_it) {}

        /** Devuelve el par (clave, valor) de la entrada actual. */
        reference operator*() const {
            auto& node = const_cast<typename Tree::Node&>(*m_it);
            return {node.getValue(), node.ref()};
        }
        /** Accede a `first` y `second` de la entrada actual. */
        Arrow operator->() const { return {**this}; }

        /** Avanza a la siguiente entrada. */
        EntryIterator& operator++() {
            ++m_it;
            return *this;
        }
        /** Avanza a la siguiente entrada y devuelve el iterador previo. */
        EntryIterator operator++(int) {
            auto old = *this;
            ++m_it;
            return old;
        }

        /** Dos iteradores son iguales si apuntan a la misma entrada. */
        friend bool operator==(const EntryIterator& a, const EntryIterator& b) { return a.m_it == b.m_it; }
    };

    /** Envuelve un iterador del árbol en un EntryIterator, constante si la tabla lo es. */
    template <typename Self, typename TreeIterator> static auto wrap(Self&, TreeIterator it) {
        return EntryIterator<TreeIterator, std::conditional_t<std::is_const_v<Self>, const Value, Value>>(it);
    }

public:
    using iterator = EntryIterator<typename Tree::ForwardIterator, Value>;
    using const_iterator = EntryIterator<typename Tree::ForwardIterator, const Value>;
    using reverse_iterator = EntryIterator<typename Tree::BackwardIterator, Value>;
    using const_reverse_iterator = EntryIterator<typename Tree::BackwardIterator, const Value>;

    /** Crea una tabla vacía. */
    HashTable() = default;
    /** Crea una tabla con los pares dados; si una clave se repite, se conserva la primera. */
    HashTable(std::initializer_list<std::pair<Key, Value>> values) {
        for (const auto& [key, value] : values)
            m_tree.insert_unique(key, value);
    }

    /** Iterador a la primera entrada en el orden de las claves; es constante si la tabla lo es. */
    template <typename Self> auto begin(this Self& self) { return wrap(self, self.m_tree.begin()); }
    /** Iterador de fin. */
    template <typename Self> auto end(this Self& self) { return wrap(self, self.m_tree.end()); }
    /** Iterador inverso a la última entrada en el orden de las claves. */
    template <typename Self> auto rbegin(this Self& self) { return wrap(self, self.m_tree.rbegin()); }
    /** Iterador inverso de fin. */
    template <typename Self> auto rend(this Self& self) { return wrap(self, self.m_tree.rend()); }

    /** Devuelve el valor de @p key, insertándolo con un valor por defecto si no existía. */
    Value& operator[](const Key& key) { return m_tree[key]; }

    /**
     * Devuelve el valor de @p key.
     * @throws std::out_of_range si la clave no existe.
     */
    template <typename Self> auto& at(this Self& self, const Key& key) {
        auto it = self.find(key);
        if (it == self.end()) throw std::out_of_range("HashTable::at");
        return it->second;
    }

    /**
     * Inserta la entrada si la clave no existía.
     * @return Iterador a la entrada insertada o existente, y si hubo inserción.
     */
    auto insert(const value_type& entry) {
        auto [it, inserted] = m_tree.try_insert(entry.first, entry.second);
        return std::pair{wrap(*this, it), inserted};
    }

    /** Construye una entrada con @p args y la inserta como insert(). */
    template <typename... Args> auto emplace(Args&&... args) { return insert(value_type(std::forward<Args>(args)...)); }

    /**
     * Busca @p key.
     * @return Iterador a la entrada, o `end()` si no existe.
     */
    template <typename Self> auto find(this Self& self, const Key& key) {
        return wrap(self, self.m_tree.locate(key));
    }

    /** Indica si existe la clave. */
    bool contains(const Key& key) const { return m_tree.contains(key); }
    /**
     * Elimina la entrada de @p key.
     * @return `false` si no existía.
     */
    bool remove(const Key& key) { return m_tree.remove(key); }

    /**
     * Elimina la entrada de @p key.
     * @return Cantidad de entradas eliminadas (0 o 1).
     */
    std::size_t erase(const Key& key) { return m_tree.remove(key); }
    /**
     * Elimina la entrada apuntada por @p it.
     * @return Iterador a la entrada siguiente.
     */
    template <typename TreeIterator, typename V> auto erase(EntryIterator<TreeIterator, V> it) {
        auto next = it++;
        m_tree.remove(next->first);
        return it;
    }

    /** Indica si la tabla no tiene entradas. */
    bool empty() const { return m_tree.empty(); }
    /** Cantidad de entradas. */
    std::size_t size() const { return m_tree.size(); }
    /** Elimina todas las entradas. */
    void clear() { m_tree.clear(); }

    /** Recorrido en preorden del árbol interno. */
    auto PreOrder() const { return m_tree.PreOrder(); }

    /** Dibuja el árbol interno. */
    void print_tree(std::ostream& os, visual::Layout layout = visual::Layout::Vertical) const {
        m_tree.print_tree(os, layout);
    }

    /** Escribe la tabla en formato `[(clave,valor),...]`. */
    friend std::ostream& operator<<(std::ostream& os, const HashTable& table) { return os << table.m_tree; }
    /** Lee la tabla desde el formato `[(clave,valor),...]`. */
    friend std::istream& operator>>(std::istream& is, HashTable& table) { return is >> table.m_tree; }
};

#endif // !__HASHTABLE_H__
