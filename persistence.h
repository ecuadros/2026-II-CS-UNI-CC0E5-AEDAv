#ifndef __PERSISTENCE_H__
#define __PERSISTENCE_H__
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>
#include <concepts>
#include <ranges>
//#include "GeneralNode.h"

template <typename T>
concept PrintableContainer =
    requires(T& container) {
        container.begin();
        container.end();
    } &&
    !std::same_as<std::remove_cvref_t<T>, std::string> &&
    !std::same_as<std::remove_cvref_t<T>, std::string_view>;

//
template <PrintableContainer Container>
std::ostream &operator<<(std::ostream &os, Container &container) {
    os << "[";
    for (auto it = container.begin(); it != container.end(); ++it) {
        os << *it;
        if (std::next(it) != container.end()) {
            os << ", ";
        }
    }
    return os << "]";
}

// Lectura generica de cualquier contenedor que defina Node, Delim,
// clear() y push(value, ref).
template <typename T>
concept ReadableContainer =
    requires {
        typename T::Node;
        typename T::Delim;
    } &&
    requires(T& container, typename T::Node& node) {
        container.clear();
        container.push(node.getValue(), node.getRef());
    };

template <ReadableContainer Container>
std::istream &operator>>(std::istream &is, Container &container) {
    using Node  = typename Container::Node;
    using Delim = typename Container::Delim;

    Delim d;
    Node node;
    container.clear();

    is >> d;
    if (is >> d && d != ']') {
        is.unget();
        while (is >> node >> d) {
            container.push(node.getValue(), node.getRef());
            if (d == ']') {
                break;
            }
        }
    }

    return is;
}

#endif // __PERSISTENCE_H__
