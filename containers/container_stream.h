#ifndef CONTAINERS_CONTAINER_STREAM_H
#define CONTAINERS_CONTAINER_STREAM_H

#include "../types.h"
#include <istream>
#include <ostream>

namespace container_stream {

using Delim = std::istream::char_type;

inline bool expect(std::istream& is, Delim wanted) {
    Delim actual;
    if (!(is >> actual)) return false;
    if (actual == wanted) return true;
    is.setstate(std::ios::failbit);
    return false;
}

template <class It> std::ostream& write(std::ostream& os, It first, It last) {
    os << Delim{'['};
    if (first != last) {
        os << *first;
        while (++first != last) os << Delim{','} << *first;
    }
    return os << Delim{']'};
}

template <class Container> bool read(std::istream& is, Container& destination) {
    if (!expect(is, '[')) return false;
    is >> std::ws;
    if (is.peek() == ']') {
        is.get();
        return true;
    }
    while (true) {
        typename Container::value_type value;
        Ref ref;
        if (!expect(is, '(') || !(is >> value) || !expect(is, ',') || !(is >> ref) || !expect(is, ')'))
            return false;
        destination.push_back(value, ref);
        Delim separator;
        if (!(is >> separator)) return false;
        if (separator == ']') return true;
        if (separator != ',') {
            is.setstate(std::ios::failbit);
            return false;
        }
    }
}
} 

template <class Container>
    requires requires { typename Container::container_stream_tag; }
std::ostream& operator<<(std::ostream& os, const Container& container) {
    return container.write(os);
}

template <class Container>
    requires requires { typename Container::container_stream_tag; }
std::istream& operator>>(std::istream& is, Container& container) {
    return container.read(is);
}

#endif
