#ifndef __GENERAL_NODE_H__
#define __GENERAL_NODE_H__
#include <cctype>
#include <iostream>
#include <string>
#include <type_traits>
#include "../types.h" // Ref

/** Delimitadores del formato de texto `[(valor,ref),...]` con el que se leen y escriben los contenedores. */
struct StreamDelimiters {
    using Delim = std::istream::char_type;
    static constexpr Delim container_open  = '[';
    static constexpr Delim container_close = ']';
    static constexpr Delim separator       = ',';
    static constexpr Delim node_open       = '(';
    static constexpr Delim node_close      = ')';

    /**
     * Lee un carácter y comprueba que sea @p wanted. Si no coincide, marca `failbit` en el stream.
     * @return `true` si se leyó el delimitador esperado.
     */
    static bool expect(std::istream& is, Delim wanted) {
        Delim actual;
        if (!(is >> actual)) return false;
        if (actual == wanted) return true;
        is.setstate(std::ios::failbit);
        return false;
    }
};

/** Tipo de nodo real en una jerarquía CRTP: @p Derived si se indicó, o @p Self si @p Derived es `void`. */
template <typename Self, typename Derived>
using NodeSelf = std::conditional_t<std::is_void_v<Derived>, Self, Derived>;

/**
 * Nodo base de todos los contenedores: un valor y una referencia asociada.
 * @tparam T Tipo del valor.
 * @tparam R Tipo de la referencia.
 */
template <typename T, typename R = Ref>
struct GeneralNode{
public:
    using ref_type = R;

private:
    T m_value;
    R m_ref{};

    /** Indica si la cadena @p text necesita comillas para no confundirse con los delimitadores. */
    static bool needs_quotes(const std::string& text) {
        return text.find_first_of("[](),\"\\") != std::string::npos || (!text.empty() && std::isspace(static_cast<unsigned char>(text[0])));
    }

    /** Escribe un campo; las cadenas con delimitadores van entre comillas con `\"` y `\\` escapados. */
    template <typename V>
    static void write_field(std::ostream& os, const V& field) {
        if constexpr (std::is_same_v<V, std::string>) {
            if (needs_quotes(field)) {
                os << '"';
                for (char c : field) {
                    if (c == '"' || c == '\\') os << '\\';
                    os << c;
                }
                os << '"';
                return;
            }
        }
        os << field;
    }

    /** Lee un campo del nodo desde el stream. Las cadenas entre comillas se leen con escapes; las demás, hasta el delimitador @p stop, sin consumirlo. El resto de tipos usa `operator>>`. */
    template <typename V>
    static bool read_field(std::istream& is, V& field, StreamDelimiters::Delim stop) {
        if constexpr (std::is_same_v<V, std::string>) {
            field.clear();
            is >> std::ws;
            char c;
            if (is.peek() == '"') {
                is.get();
                while (is.get(c)) {
                    if (c == '"') return true;
                    if (c == '\\' && !is.get(c)) break;
                    field.push_back(c);
                }
                is.setstate(std::ios::failbit);
                return false;
            }
            while (is.get(c)) {
                if (c == stop) {
                    is.unget();
                    return true;
                }
                field.push_back(c);
            }
            is.setstate(std::ios::failbit);
            return false;
        } else {
            return static_cast<bool>(is >> field);
        }
    }

public:
    /** Crea un nodo con valor y referencia por defecto. */
    GeneralNode() = default;
    /** Crea un nodo con el valor y la referencia dados. */
    GeneralNode(const T& value, const R& ref) : m_value(value), m_ref(ref) {};
    /** Devuelve el valor en modo lectura. */
    const T& getValue() const { return m_value; }
    /** Devuelve la referencia en modo lectura. */
    const R& getRef()   const { return m_ref;   }
    /** Devuelve el valor de forma modificable. */
    T&   value()          { return m_value; } // acceso mutable para ApplyFunction
    /** Devuelve la referencia de forma modificable. */
    R&   ref()            { return m_ref;   }

    using Delim = StreamDelimiters::Delim;

    /** Escribe el nodo como `(valor,ref)`; las cadenas con delimitadores van entre comillas. */
    friend std::ostream &operator <<(std::ostream &os, const GeneralNode<T, R> &node) {
        os << StreamDelimiters::node_open;
        write_field(os, node.getValue());
        os << StreamDelimiters::separator;
        write_field(os, node.getRef());
        return os << StreamDelimiters::node_close;
    }

    /** Lee un nodo con formato `(valor,ref)`; si el formato no coincide, el stream queda en fallo. */
    friend std::istream &operator >>(std::istream &is, GeneralNode<T, R> &node){
        using SD = StreamDelimiters;
        static_cast<void>(SD::expect(is, SD::node_open) && read_field(is, node.m_value, SD::separator) &&
                          SD::expect(is, SD::separator) && read_field(is, node.m_ref, SD::node_close) &&
                          SD::expect(is, SD::node_close));
        return is;
    }
};

#endif // __GENERAL_NODE_H__
