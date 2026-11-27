#ifndef CONTAINERS_CONTAINER_STREAM_H
#define CONTAINERS_CONTAINER_STREAM_H

#include "../../types.h"
#include "GeneralNode.h"
#include <concepts>
#include <istream>
#include <ostream>
#include <utility>
#include <vector>

/** Lectura y escritura de contenedores en formato de texto `[(valor,ref),...]`. */
namespace container_stream {

/** Tipo de referencia de un contenedor: `C::ref_type` si existe, o `Ref` en caso contrario. */
template <class C> struct ref_of { using type = Ref; };
/** Especialización para los contenedores que definen `ref_type`. */
template <class C> requires requires { typename C::ref_type; }
struct ref_of<C> { using type = typename C::ref_type; };
/** Atajo para `ref_of<C>::type`. */
template <class C> using ref_of_t = typename ref_of<C>::type;

using Delim = StreamDelimiters::Delim;

/** Contenedor que se puede recorrer con `begin()`/`end()` y cuyos nodos se pueden escribir en un stream. */
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

/** Contenedor imprimible que además se puede vaciar y rellenar con `clear()` y `push_back(valor, ref)`. */
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

/** Tipo que delega su escritura en stream a `write_stream(os)`. */
template <class T>
concept StreamWritable =
    requires(const T& obj, std::ostream& os) {
        { obj.write_stream(os) } -> std::same_as<std::ostream&>;
    };

/** Tipo que delega su lectura desde stream a `read_stream(is)`. */
template <class T>
concept StreamReadable =
    requires(T& obj, std::istream& is) {
        { obj.read_stream(is) } -> std::same_as<std::istream&>;
    };

/** Escribe el contenedor como `[nodo,nodo,...]`. */
template <PrintableContainer Container>
std::ostream& write(std::ostream& os, const Container& container) {
    os << StreamDelimiters::container_open;
    Bool first = true;
    container.call([&](const auto& node) {
        if (!first) os << StreamDelimiters::separator;
        first = false;
        os << node;
    });
    return os << StreamDelimiters::container_close;
}

/** Escribe un objeto `StreamWritable` delegando en `write_stream(os)`. */
template <StreamWritable Target>
    requires (!PrintableContainer<Target>)
std::ostream& write(std::ostream& os, const Target& target) {
    return target.write_stream(os);
}

/**
 * Lee una lista `[nodo,nodo,...]` y la acumula en @p nodes.
 * @return `false` si el formato es inválido; el stream queda en fallo.
 */
template <typename T, typename R, typename Nodes>
Bool read_nodes(std::istream& is, Nodes& nodes) {
    if (!StreamDelimiters::expect(is, StreamDelimiters::container_open)) return false;
    is >> std::ws;
    if (is.peek() == StreamDelimiters::container_close) {
        is.get();
        return true;
    }
    while (true) {
        GeneralNode<T, R> node;
        if (!(is >> node)) return false;
        if constexpr (requires { nodes.push_back(node); }) nodes.push_back(node);
        else nodes.push_back(node.getValue(), node.getRef());
        Delim separator;
        if (!(is >> separator)) return false;
        if (separator == StreamDelimiters::container_close) return true;
        if (separator != StreamDelimiters::separator) {
            is.setstate(std::ios::failbit);
            return false;
        }
    }
}

/** Reemplaza el contenido de @p destination por @p nodes. Construye primero un contenedor temporal, así que si falla el destino no cambia. */
template <ReadableContainer Container>
void insert_nodes(Container& destination,
                  const std::vector<GeneralNode<typename Container::value_type, ref_of_t<Container>>>& nodes) {
    Container temp;
    for (const auto& node : nodes)
        temp.push_back(node.getValue(), node.getRef());
    destination = std::move(temp);
}

/** Lee un contenedor completo desde el stream. Si el texto es inválido, el contenedor no se modifica. */
template <ReadableContainer Container>
std::istream& read(std::istream& is, Container& container) {
    std::vector<GeneralNode<typename Container::value_type, ref_of_t<Container>>> nodes;
    if (read_nodes<typename Container::value_type, ref_of_t<Container>>(is, nodes))
        insert_nodes(container, nodes);
    return is;
}

/** Lee un objeto `StreamReadable` delegando en `read_stream(is)`. */
template <StreamReadable Target>
    requires (!ReadableContainer<Target>)
std::istream& read(std::istream& is, Target& target) {
    return target.read_stream(is);
}

} // namespace container_stream

#endif
