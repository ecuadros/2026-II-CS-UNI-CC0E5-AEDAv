#ifndef CONTAINERS_CONTAINER_STREAM_H
#define CONTAINERS_CONTAINER_STREAM_H

#include "../types.h"
#include <istream>
#include <ostream>
#include <utility>
#include <vector>

namespace container_stream {

template <class It>
std::ostream& write(std::ostream& os, It first, It last) {
    os << '[';
    for (bool separator = false; first != last; ++first) {
        if (separator) os << ',';
        os << *first;
        separator = true;
    }
    return os << ']';
}

template <class T>
bool read(std::istream& is, std::vector<std::pair<T, Ref>>& values) {
    auto expect = [&is](char wanted) {
        char actual;
        if (!(is >> actual)) return false;
        if (actual == wanted) return true;
        is.setstate(std::ios::failbit);
        return false;
    };
    if (!expect('[')) return false;
    is >> std::ws;
    if (is.peek() == ']') {
        is.get();
        return true;
    }
    while (true) {
        T value;
        Ref ref;
        if (!expect('(') || !(is >> value) || !expect(',') || !(is >> ref) || !expect(')'))
            return false;
        values.emplace_back(std::move(value), ref);
        char separator;
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
