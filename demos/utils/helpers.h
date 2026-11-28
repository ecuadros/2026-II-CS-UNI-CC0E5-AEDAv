/** @file
 * Ayudantes generales de las demos: impresión, archivos temporales, hilos y comprobaciones.
 */
#pragma once

#include "concepts.h"

/** Devuelve la clave de un valor: `value.key()` si existe, o el propio valor. */
template <typename T> auto KeyOf(const T& value) {
    if constexpr (requires { value.key(); })
        return value.key();
    else
        return value;
}

/** Suma 1 al valor del nodo. */
inline void AddOne(GeneralNode<TX>& node) {
    node.value() += 1;
}

/** Suma @p x al valor del nodo. */
template <typename T> void AddX(GeneralNode<T>& node, T x) {
    node.value() += x;
}

/** Eleva al cuadrado el valor del nodo. */
inline void Square(GeneralNode<TX>& node) {
    node.value() *= node.value();
}

/** Escribe el nodo seguido de un espacio. */
template <typename Node> void PrintNode(const Node& node, std::ostream& os) {
    os << node << " ";
}
/** Cantidad de hilos de las pruebas de concurrencia. */
const Int NThreads = 5;

/** Imprime @p value en una línea con el color ANSI @p color. */
template <typename Value>
void PrintColor(const Value& value, std::string_view color = "1;36", std::ostream& os = std::cout) {
    os << visual::Painter(os)(color, visual::to_text(value)) << '\n';
}

/** Ejecuta @p function y borra el archivo @p filename al terminar, también si lanza. */
template <typename Function> void WithDemoFile(const std::string& filename, Function function) {
    auto removeFile = [&filename] {
        if (std::remove(filename.c_str()) != 0 && errno != ENOENT)
            std::cerr << "No se pudo borrar " << filename << '\n';
    };
    try {
        function();
    } catch (...) {
        removeFile();
        throw;
    }
    removeFile();
}

/**
 * Escribe el contenedor en un archivo.
 * @throws std::runtime_error si no se puede crear o escribir.
 */
template <typename Container> void SaveTo(const std::string& filename, const Container& container) {
    std::ofstream file(filename, std::ios::trunc);
    if (!file) throw std::runtime_error("No se pudo crear " + filename);
    file << container << '\n';
    if (!file) throw std::runtime_error("No se pudo escribir " + filename);
}

/**
 * Lee el contenedor desde un archivo.
 * @throws std::runtime_error si no se puede abrir o leer.
 */
template <typename Container> void LoadFrom(const std::string& filename, Container& container) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("No se pudo abrir " + filename);
    if (!(file >> container)) throw std::runtime_error("No se pudo leer " + filename);
}

/** Inserta con `insert` todos los pares de @p values. */
template <typename Container> void InsertAll(Container& container, const Values<Container>& values) {
    for (const auto& [value, ref] : values)
        container.insert(value, ref);
}

/** Par de iteradores utilizable en un `for` por rango. */
template <typename Iterator> struct Range {
    Iterator first, last;
    Iterator begin() const { return first; }
    Iterator end() const { return last; }
};

/** Devuelve el rango inverso (`rbegin()`, `rend()`) del contenedor. */
template <typename Container> auto ReverseRange(const Container& container) {
    return Range{container.rbegin(), container.rend()};
}

/** Imprime `etiqueta: [ ... ]`, donde el contenido lo escribe @p body. */
template <typename Body> void PrintBracketed(const std::string& label, Body body) {
    std::cout << label << ": [";
    body();
    std::cout << "]\n";
}

/** Imprime todos los nodos de un recorrido entre corchetes. */
template <typename Traversal>
void PrintTraversal(const std::string& label, const Traversal& traversal) {
    PrintBracketed(label, [&] {
        for (auto&& node : traversal)
            std::cout << node << " ";
    });
}

/** Devuelve las referencias de los nodos en el orden del recorrido. */
template <typename Traversal> std::vector<Ref> CollectRefs(const Traversal& traversal) {
    std::vector<Ref> refs;
    for (auto&& node : traversal)
        refs.push_back(node.getRef());
    return refs;
}

/**
 * Comprueba que @p backward sea exactamente @p forward al revés.
 * @throws std::runtime_error si no coinciden.
 */
template <typename Forward, typename Backward>
void CheckReverse(const std::string& name, const Forward& forward, const Backward& backward) {
    std::vector<Ref> refs = CollectRefs(backward);
    std::reverse(refs.begin(), refs.end());
    if (refs != CollectRefs(forward))
        throw std::runtime_error(name + ": el recorrido inverso no coincide");
}

/** Imprime una vuelta de una lista circular como `a -> b -> (vuelve a a)`. */
template <typename Iterator> void PrintCircularTraversal(Iterator begin, Iterator end) {
    if (begin == end) {
        std::cout << "[]\n";
        return;
    }

    std::cout << *begin;
    auto it = begin;
    for (++it; it != end; ++it)
        std::cout << " -> " << *it;
    std::cout << " -> (vuelve a " << *begin << ")\n";
}

/** Ejecuta @p workers hilos con @p body(indice) y espera a todos. */
template <typename Body> void RunThreads(Int workers, Body body) {
    std::vector<std::thread> pool;
    for (Int t = 0; t < workers; ++t)
        pool.emplace_back(body, t);
    for (auto& th : pool)
        th.join();
}

/** Lanza `std::runtime_error` con el mensaje `nombre: motivo` si @p ok es falso. */
inline void Check(Bool ok, const std::string& name, const std::string& what) {
    if (!ok) throw std::runtime_error(name + ": " + what);
}

/** Comprueba una condición de los demos nuevos. @throws std::runtime_error con @p what si falla. */
static void Expect(Bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error(what);
}

/** Pares (valor, ref) del contenedor ordenados, para comparar contenidos sin depender del orden interno. */
template <typename Container> std::vector<std::pair<typename Container::value_type, Ref>> Sorted(const Container& c) {
    std::vector<std::pair<typename Container::value_type, Ref>> items;
    c.call([&](const auto& node) { items.emplace_back(node.getValue(), node.getRef()); });
    std::sort(items.begin(), items.end());
    return items;
}
