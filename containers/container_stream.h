#ifndef CONTAINERS_CONTAINER_STREAM_H
#define CONTAINERS_CONTAINER_STREAM_H

#include "GeneralNode.h"
#include <concepts>
#include <istream>
#include <ostream>
#include <utility>
#include <vector>

namespace container_stream {

template <class C> struct ref_of { using type = Ref; };
template <class C> requires requires { typename C::ref_type; }
struct ref_of<C> { using type = typename C::ref_type; };
template <class C> using ref_of_t = typename ref_of<C>::type;

using Delim = StreamDelimiters::Delim;

template <class Container>
concept PrintableContainer =
    requires(const Container& c, std::ostream& os) {
        typename Container::Node;
        c.begin();
        c.end();
        requires std::same_as<decltype(c.begin()), decltype(c.end())>;
        c.begin() != c.end();
        ++c.begin();
        os << *c.begin();
    };

template <class Container>
concept ReadableContainer =
    PrintableContainer<Container> &&
    requires(Container& c, typename Container::value_type value, ref_of_t<Container> ref,
             std::istream& is, GeneralNode<typename Container::value_type, ref_of_t<Container>>& node) {
        typename Container::value_type;
        requires std::default_initializable<GeneralNode<typename Container::value_type, ref_of_t<Container>>>;
        is >> node;
        c.clear();
        c.push_back(value, ref);
    };

template <PrintableContainer Container>
std::ostream& write(std::ostream& os, const Container& container) {
    os << StreamDelimiters::container_open;
    bool first = true;
    container.call([&](const auto& node) {
        if (!first) os << StreamDelimiters::separator;
        first = false;
        os << node;
    });
    return os << StreamDelimiters::container_close;
}

template <typename T, typename R>
bool read_nodes(std::istream& is, std::vector<GeneralNode<T, R>>& nodes) {
    if (!StreamDelimiters::expect(is, StreamDelimiters::container_open)) return false;
    is >> std::ws;
    if (is.peek() == StreamDelimiters::container_close) {
        is.get();
        return true;
    }
    while (true) {
        GeneralNode<T, R> node;
        if (!(is >> node)) return false;
        nodes.push_back(node);
        Delim separator;
        if (!(is >> separator)) return false;
        if (separator == StreamDelimiters::container_close) return true;
        if (separator != StreamDelimiters::separator) {
            is.setstate(std::ios::failbit);
            return false;
        }
    }
}

template <ReadableContainer Container>
void insert_nodes(Container& destination,
                  const std::vector<GeneralNode<typename Container::value_type, ref_of_t<Container>>>& nodes) {
    Container temp;
    for (const auto& node : nodes)
        temp.push_back(node.getValue(), node.getRef());
    destination = std::move(temp);
}

template <ReadableContainer Container>
std::istream& read(std::istream& is, Container& container) {
    std::vector<GeneralNode<typename Container::value_type, ref_of_t<Container>>> nodes;
    if (read_nodes(is, nodes))
        insert_nodes(container, nodes);
    return is;
}

} // namespace container_stream

#endif
