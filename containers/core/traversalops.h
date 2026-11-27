#ifndef TRAVERSALOPS_H
#define TRAVERSALOPS_H

#include "../../types.h"
#include "container_stream.h"
#include "traversal_algorithms.h"
#include "../visual/visualization.h"
#include <fstream>
#include <istream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <mutex>
#include <utility>

/** Inserta en @p container todos los pares (valor, ref) de @p values. Si una inserción lanza, vacía el contenedor y relanza la excepción. */
template <class Container, class Values>
void fill_from(Container& container, const Values& values) {
    try {
        for (const auto& [value, ref] : values)
            container.push_back(value, ref);
    } catch (...) {
        container.clear();
        throw;
    }
}

/** Contenedor que se puede recorrer hacia atrás con `rbegin()`/`rend()`. */
template <class Container>
concept ReverseTraversable = requires(Container& container) {
    container.rbegin();
    container.rend();
};

/** Mixin que aporta el mutex recursivo con el que los contenedores protegen sus operaciones, de modo que una función pasada a `call` puede usar el mismo contenedor. Al copiar o asignar, el mutex no se copia: cada objeto conserva el suyo. */
class LockableOp {
    friend struct LockedTraversal;

protected:
    /** Mutex del contenedor. */
    mutable std::recursive_mutex m_mutex;

public:
    /** Crea el mixin con su propio mutex. */
    LockableOp() = default;
    /** Copia sin copiar el mutex. */
    LockableOp(const LockableOp&) noexcept {}
    /** Asigna sin tocar el mutex. */
    LockableOp& operator=(const LockableOp&) noexcept { return *this; }
};

/** Ayudante que ejecuta algoritmos de recorrido con el mutex del contenedor tomado. */
struct LockedTraversal {
    /** Toma el mutex de @p c mientras viva el objeto devuelto. */
    template <class Self>
    static std::lock_guard<std::recursive_mutex> guard(Self& c) {
        return std::lock_guard<std::recursive_mutex>(c.m_mutex);
    }

    /** Ejecuta @p algorithm sobre todo el contenedor con el mutex tomado. Si @p Reverse es `true`, recorre de `rbegin()` a `rend()`. */
    template <Bool Reverse, class Self, class Algorithm, class... Args>
    static decltype(auto) run(Self& c, Algorithm algorithm, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        if constexpr (Reverse)
            return algorithm(c.rbegin(), c.rend(), std::forward<Args>(args)...);
        else
            return algorithm(c.begin(), c.end(), std::forward<Args>(args)...);
    }
};

/** Mixin que aporta `FirstThat`. */
class FirstThatOp {
public:
    /**
     * Devuelve el primer elemento que cumple @p pred, recorriendo hacia adelante.
     * @throws std::runtime_error si ninguno lo cumple.
     */
    template <class Self, class Pred, class... Args>
    decltype(auto) FirstThat(this Self& c, Pred&& pred, Args&&... args) {
        return LockedTraversal::run<false>(c, traversal_detail::first_that,
                                           std::forward<Pred>(pred), std::forward<Args>(args)...);
    }
};

/** Mixin que aporta `RFirstThat`. */
class RFirstThatOp {
public:
    /**
     * Devuelve el primer elemento que cumple @p pred, recorriendo hacia atrás.
     * @throws std::runtime_error si ninguno lo cumple.
     */
    template <class Self, class Pred, class... Args>
        requires ReverseTraversable<Self>
    decltype(auto) RFirstThat(this Self& c, Pred&& pred, Args&&... args) {
        return LockedTraversal::run<true>(c, traversal_detail::first_that,
                                          std::forward<Pred>(pred), std::forward<Args>(args)...);
    }
};

/** Mixin que aporta `call`. */
class CallOp {
public:
    /** Recorre hacia adelante aplicando @p func. Si @p func devuelve `void` se aplica a todos los elementos; si devuelve un booleano, se detiene y devuelve el primero que lo cumpla. */
    template <class Self, class Func, class... Args>
    decltype(auto) call(this Self& c, Func&& func, Args&&... args) {
        return LockedTraversal::run<false>(c, traversal_detail::legacy_call,
                                           std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

/** Mixin que aporta `rcall`. */
class RCallOp {
public:
    /** Igual que `call`, pero recorriendo hacia atrás. */
    template <class Self, class Func, class... Args>
        requires ReverseTraversable<Self>
    decltype(auto) rcall(this Self& c, Func&& func, Args&&... args) {
        return LockedTraversal::run<true>(c, traversal_detail::legacy_call,
                                          std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

/** Mixin que aporta `ApplyFunction`. */
class ApplyFunctionOp {
public:
    /** Aplica @p func a cada elemento, que puede modificarlo. */
    template <class Self, class Func, class... Args>
    void ApplyFunction(this Self& c, Func&& func, Args&&... args) {
        LockedTraversal::run<false>(c, traversal_detail::for_each,
                                    std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

/** Mixin que habilita `operator<<` para el contenedor. */
class PrintableOp {
    /** Escribe el contenedor en formato `[(valor,ref),...]`. */
    template <class Container>
        requires container_stream::PrintableContainer<Container> || container_stream::StreamWritable<Container>
    friend std::ostream& operator<<(std::ostream& os, const Container& container) {
        return container_stream::write(os, container);
    }
};

/** Mixin que habilita `operator>>` para el contenedor. */
class ReadableOp {
    /** Lee el contenedor desde el formato `[(valor,ref),...]`, reemplazando su contenido. */
    template <class Container>
        requires container_stream::ReadableContainer<Container> || container_stream::StreamReadable<Container>
    friend std::istream& operator>>(std::istream& is, Container& container) {
        return container_stream::read(is, container);
    }
};

/** Mixin que dibuja el contenedor en la terminal. */
class VisualOp {
public:
    /** Dibuja el contenedor como árbol con la disposición @p layout. */
    template <class Self>
        requires visual::HasTreeView<Self>
    void print_tree(this const Self& self, std::ostream& os,
                    visual::Layout layout = visual::Layout::Vertical) {
        auto lock = LockedTraversal::guard(self);
        visual::print_tree(os, self.tree_view(), layout);
    }

    /** Dibuja el contenedor como secuencia: cadena de nodos para las listas, celdas para los arreglos. */
    template <class Self>
        requires visual::LinearView<Self>
    void print_linear(this const Self& self, std::ostream& os) {
        auto lock = LockedTraversal::guard(self);
        visual::print_linear(os, self);
    }

    /** Dibuja el contenedor como tabla con columnas y filas numeradas; asume que `table_view()` es thread-safe (snapshot). */
    template <class Self>
        requires visual::HasTableView<Self>
    void print_table(this const Self& self, std::ostream& os) {
        visual::print_table(os, self.table_view());
    }

    /** Dibuja el contenedor como árbol, como tabla, como secuencia o con `print(os)`. */
    template <class Self>
        requires visual::HasTreeView<Self> || visual::HasTableView<Self> || visual::LinearView<Self> ||
                 visual::PrintableView<Self>
    void visualize(this const Self& self, std::ostream& os) {
        if constexpr (visual::HasTreeView<Self>)
            self.print_tree(os);
        else if constexpr (visual::HasTableView<Self>)
            self.print_table(os);
        else if constexpr (visual::LinearView<Self>)
            self.print_linear(os);
        else
            self.print(os);
    }
};

/**
 * Mixin que guarda y lee el contenedor en archivos de texto, de dos maneras:
 * `save`/`load` con el formato `[(valor,ref),...]` y `save_records`/`load_records` con una línea `id valor` por elemento.
 * Todos los métodos lanzan `std::runtime_error` si el archivo no se puede abrir o tiene un formato inválido.
 */
class FileOp {
    /** Quita el retorno de carro final de una línea leída en un archivo de Windows. */
    static void trim_line(std::string& line) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
    }

public:
    /** Escribe el contenedor en @p path con el formato `[(valor,ref),...]`, reemplazando el archivo. */
    template <class Self>
        requires container_stream::PrintableContainer<Self> || container_stream::StreamWritable<Self>
    void save(this const Self& self, const std::string& path) {
        std::ofstream file(path, std::ios::trunc);
        if (!file) throw std::runtime_error("No se pudo crear " + path);
        container_stream::write(file, self);
        file << '\n';
        if (!file) throw std::runtime_error("No se pudo escribir " + path);
    }

    /** Reemplaza el contenido por el que hay en @p path en el formato de `save`; si el texto es inválido no cambia. */
    template <class Self>
        requires container_stream::ReadableContainer<Self> || container_stream::StreamReadable<Self>
    void load(this Self& self, const std::string& path) {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("No se pudo abrir " + path);
        container_stream::read(file, self);
        if (file.fail()) throw std::runtime_error("Formato invalido en " + path);
    }

    /** Escribe en @p path una línea `id valor` por elemento, donde el id es la referencia del nodo. */
    template <class Self>
        requires container_stream::PrintableContainer<Self>
    void save_records(this const Self& self, const std::string& path) {
        std::ofstream file(path, std::ios::trunc);
        if (!file) throw std::runtime_error("No se pudo crear " + path);
        self.call([&](const auto& node) { file << node.getRef() << ' ' << node.getValue() << '\n'; });
        if (!file) throw std::runtime_error("No se pudo escribir " + path);
    }

    /**
     * Reemplaza el contenido por las líneas `id valor` de @p path; ignora las vacías y las que empiezan con `#`.
     * Para cadenas, el valor es el resto de la línea. Si alguna línea es inválida, el contenedor no cambia.
     * @return Cantidad de registros leídos.
     */
    template <class Self>
        requires container_stream::ReadableContainer<Self>
    std::size_t load_records(this Self& self, const std::string& path) {
        using Value = typename Self::value_type;
        std::ifstream file(path);
        if (!file) throw std::runtime_error("No se pudo abrir " + path);
        Self loaded;
        std::size_t count = 0, number = 0;
        for (std::string line; std::getline(file, line);) {
            ++number;
            trim_line(line);
            if (line.empty() || line[0] == '#') continue;
            std::istringstream in(line);
            container_stream::ref_of_t<Self> ref;
            Value value;
            Bool ok = static_cast<Bool>(in >> ref);
            if constexpr (std::is_same_v<Value, std::string>) {
                in >> std::ws;
                value.assign(std::istreambuf_iterator<Char>(in), std::istreambuf_iterator<Char>());
            } else {
                ok = ok && static_cast<Bool>(in >> value);
            }
            if (!ok) throw std::runtime_error(path + ":" + std::to_string(number) + ": registro invalido");
            loaded.push_back(value, ref);
            ++count;
        }
        self = std::move(loaded);
        return count;
    }
};

/**
 * Combina los mixins indicados en una sola base y añade siempre LockableOp.
 * @tparam Ops Mixins de operaciones que heredará el contenedor.
 */
template <class... Ops>
class TraversalOps : public LockableOp, public Ops... {};

/** Operaciones para contenedores que solo se recorren hacia adelante, con `ApplyFunction`. */
using ForwardTraversalOps = TraversalOps<CallOp, FirstThatOp, ApplyFunctionOp,
                                         PrintableOp, ReadableOp, VisualOp, FileOp>;

/** Operaciones para contenedores con recorrido en ambos sentidos, con `ApplyFunction`. */
using FullTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp, RFirstThatOp,
                                      ApplyFunctionOp, PrintableOp, ReadableOp, VisualOp, FileOp>;

/** Operaciones para contenedores que solo se recorren hacia adelante, sin `ApplyFunction`. */
using ForwardSearchTraversalOps = TraversalOps<CallOp, FirstThatOp, PrintableOp,
                                               ReadableOp, VisualOp, FileOp>;

/** Operaciones para contenedores con recorrido en ambos sentidos, sin `ApplyFunction`. */
using SearchTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp, RFirstThatOp,
                                        PrintableOp, ReadableOp, VisualOp, FileOp>;

/** Operaciones del árbol de una fórmula (ExprTree): recorridos, impresión y dibujo; no se lee desde un stream. */
using ExprTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp, RFirstThatOp, PrintableOp, VisualOp, FileOp>;

/** Operaciones de los contenedores que se serializan con `write_stream`/`read_stream` y se dibujan: grafos y árboles espaciales. */
using SerializableTraversalOps = TraversalOps<PrintableOp, ReadableOp, VisualOp, FileOp>;
/** Operaciones de la hoja de cálculo (Sheet): bloqueo, serialización (`<<`/`>>`) y visualización. */
using SheetTraversalOps = TraversalOps<PrintableOp, ReadableOp, VisualOp, FileOp>;

#endif // TRAVERSALOPS_H
