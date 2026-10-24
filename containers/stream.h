#ifndef __STREAM_H__
#define __STREAM_H__
#include <iostream>
#include "GeneralContainer.h"

template <typename Derived>
std::ostream &operator<<(std::ostream &os, const GeneralContainer<Derived> &base) {
    const Derived &container = static_cast<const Derived &>(base);
    std::lock_guard<std::mutex> lock(container.GetMutex());
    auto first = true;
    os << "[";
    for (auto it = container.begin(); it != container.end(); ++it) {
        if (!first)
            os << ",";
        os << *it;
        first = false;
    }
    return os << "]";
}

template <typename Derived>
std::istream &operator>>(std::istream &is, GeneralContainer<Derived> &base) {
    Derived &container = static_cast<Derived &>(base);
    typename Derived::Node::Delim d;
    typename Derived::Node node;
    container.clear();

    if (!(is >> d) || d != '[') {
        is.setstate(std::ios::failbit);
        return is;
    }
    if (is >> d && d != ']') {
        is.unget();
        while (is >> node >> d) {
            container.push_back(node.getValue(), node.getRef());
            if (d == ']')
                break;
        }
    }
    return is;
}

#endif // __STREAM_H__
