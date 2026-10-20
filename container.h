#ifndef CONTAINER_H
#define CONTAINER_H

#include <ostream>
#include <istream>


template <typename Container>
auto operator<<(std::ostream& os, const Container& container)
    -> decltype(container.write(os))
{
    return container.write(os);
}
//
template <typename Container>
std::istream& ReadContainer(std::istream& is, Container& container) {
    using Node = typename Container::Node;
    using Delim = typename Node::Delim;

    Delim d;
    Node node;
    container.clear();

    is >> d;
    if (is >> d && d != ']') {
        is.unget();

        while (is >> node >> d) {
            container.push_back(node.getValue(), node.getRef());

            if (d == ']') {
                break;
            }
        }
    }

    return is;
}

template <typename Container>
auto operator>>(std::istream& is, Container& container)
    -> decltype(container.read(is))
{
    return container.read(is);
}

#endif // CONTAINER_H