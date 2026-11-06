#ifndef __GENERAL_NODE_H__
#define __GENERAL_NODE_H__
#include <iostream>
#include <string>
#include <type_traits>
#include "../types.h" // Ref

struct StreamDelimiters {
    using Delim = std::istream::char_type;
    static constexpr Delim container_open  = '[';
    static constexpr Delim container_close = ']';
    static constexpr Delim separator       = ',';
    static constexpr Delim node_open       = '(';
    static constexpr Delim node_close      = ')';

    static bool expect(std::istream& is, Delim wanted) {
        Delim actual;
        if (!(is >> actual)) return false;
        if (actual == wanted) return true;
        is.setstate(std::ios::failbit);
        return false;
    }
};

template <typename Self, typename Derived>
using NodeSelf = std::conditional_t<std::is_void_v<Derived>, Self, Derived>;

template <typename T, typename R = Ref>
struct GeneralNode{
public:
    using ref_type = R;

private:
    T m_value;
    R m_ref{};

    template <typename V>
    static bool read_field(std::istream& is, V& field, StreamDelimiters::Delim stop) {
        if constexpr (std::is_same_v<V, std::string>) {
            field.clear();
            is >> std::ws;
            char c;
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
    GeneralNode() = default;
    GeneralNode(const T& value, const R& ref) : m_value(value), m_ref(ref) {};
    const T& getValue() const { return m_value; }
    const R& getRef()   const { return m_ref;   }
    T&   value()          { return m_value; } // acceso mutable para ApplyFunction
    R&   ref()            { return m_ref;   }

    using Delim = StreamDelimiters::Delim;

    friend std::ostream &operator <<(std::ostream &os, const GeneralNode<T, R> &node) {
        os << StreamDelimiters::node_open
           << node.getValue()
           << StreamDelimiters::separator
           << node.getRef()
           << StreamDelimiters::node_close;
        return os;
    }

    friend std::istream &operator >>(std::istream &is, GeneralNode<T, R> &node){
        using SD = StreamDelimiters;
        static_cast<void>(SD::expect(is, SD::node_open) && read_field(is, node.m_value, SD::separator) &&
                          SD::expect(is, SD::separator) && read_field(is, node.m_ref, SD::node_close) &&
                          SD::expect(is, SD::node_close));
        return is;
    }
};

#endif // __GENERAL_NODE_H__
