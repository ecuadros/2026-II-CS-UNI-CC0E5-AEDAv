#ifndef __GENERAL_NODE_H__
#define __GENERAL_NODE_H__
#include <iostream>
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

template <typename T>
struct GeneralNode{
private:
    T   m_value;
    Ref m_ref;      // Reference to the value

public:
    GeneralNode() = default; 
    GeneralNode(const T& value, Ref ref) : m_value(value), m_ref(ref) {};
    const T& getValue() const { return m_value; }
    Ref  getRef()   const { return m_ref;   }
    T&   value()          { return m_value; } // acceso mutable para ApplyFunction

    using Delim = StreamDelimiters::Delim;

    friend std::ostream &operator <<(std::ostream &os, const GeneralNode<T> &node) {
        os << StreamDelimiters::node_open
           << node.getValue()
           << StreamDelimiters::separator
           << node.getRef()
           << StreamDelimiters::node_close;
        return os;
    }

    friend std::istream &operator >>(std::istream &is, GeneralNode<T> &node){
        using SD = StreamDelimiters;
        static_cast<void>(SD::expect(is, SD::node_open) && is >> node.m_value &&
                          SD::expect(is, SD::separator) && is >> node.m_ref &&
                          SD::expect(is, SD::node_close));
        return is;
    }
};

#endif // __GENERAL_NODE_H__
