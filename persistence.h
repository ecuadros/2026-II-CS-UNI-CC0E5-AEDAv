#ifndef __PERSISTENCE_H__
#define __PERSISTENCE_H__
#include <iostream>
#include <concepts>
#include <ranges>
//#include "GeneralNode.h"

/*template <typename T>
concept PrintableContainer =
    std::ranges::input_range<T> &&
    !std::same_as<T, std::string> &&
    !std::same_as<T, std::string_view>;*/

/*template<typename Container>
    std::ostream& write(std::ostream& os, const Container &container) { return os << container; }*/

template<typename Iterator>
//requires (!std::is_array_v<Container> && !std::is_pointer_v<Container>)
std::ostream &write (std::ostream &os, Iterator begin, Iterator end) {
    os << "[";
    for (auto it = begin; it != end; ++it) {
        os << *it;
        if (std::next(it) != end) {
            os << ", ";
        }
    }
    return os << "]";
}

template<typename Container, typename Delim, typename Node>
std::istream &read(std::istream &is, const Container &container) {
    Delim d;
    Node node;
    container.clear();

    is >> d;
    if (is >> d && d != ']'){
        is.unget();
        while(is >> node >> d){
            container.push(node.getValue(), node.getRef());
            if (d == ']'){
                break;
            }
        }
    }

    return is;
    }

#endif // __PERSISTENCE_H__
