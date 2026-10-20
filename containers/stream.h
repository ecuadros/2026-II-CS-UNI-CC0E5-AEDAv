#ifndef __STREAM_H__
#define __STREAM_H__
#include <iostream>

template <typename Container>
concept Writable = requires(const Container &container, std::ostream &os) { container.write(os); };

template <typename Container>
concept Readable = requires(Container &container, std::istream &is) { container.read(is); };

template <Writable Container>
std::ostream &operator<<(std::ostream &os, const Container &container) {
    return container.write(os);
}

template <Readable Container>
std::istream &operator>>(std::istream &is, Container &container) {
    return container.read(is);
}

template <typename Container>
std::istream &ReadContainer(std::istream &is, Container &container) {
    typename Container::Node::Delim d;
    typename Container::Node node;
    container.clear();

    is >> d;
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
