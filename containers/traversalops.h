#ifndef TRAVERSALOPS_H
#define TRAVERSALOPS_H

#include "container_stream.h"
#include "traversal_algorithms.h"
#include "visualization.h"
#include <istream>
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

/** Mixin que aporta el mutex con el que los contenedores protegen sus operaciones. Al copiar o asignar, el mutex no se copia: cada objeto conserva el suyo. */
class LockableOp {
    friend struct LockedTraversal;

protected:
    /** Mutex del contenedor. */
    mutable std::mutex m_mutex;

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
    static std::lock_guard<std::mutex> guard(Self& c) {
        return std::lock_guard<std::mutex>(c.m_mutex);
    }

    /** Ejecuta @p algorithm sobre todo el contenedor con el mutex tomado. Si @p Reverse es `true`, recorre de `rbegin()` a `rend()`. */
    template <bool Reverse, class Self, class Algorithm, class... Args>
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
    template <container_stream::PrintableContainer Container>
    friend std::ostream& operator<<(std::ostream& os, const Container& container) {
        return container_stream::write(os, container);
    }
};

/** Mixin que habilita `operator>>` para el contenedor. */
class ReadableOp {
    /** Lee el contenedor desde el formato `[(valor,ref),...]`, reemplazando su contenido. */
    template <container_stream::ReadableContainer Container>
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

    /** Dibuja el contenedor como árbol si tiene vista de árbol, y como secuencia en caso contrario. */
    template <class Self>
        requires visual::HasTreeView<Self> || visual::LinearView<Self>
    void visualize(this const Self& self, std::ostream& os) {
        if constexpr (visual::HasTreeView<Self>)
            self.print_tree(os);
        else
            self.print_linear(os);
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
                                         PrintableOp, ReadableOp, VisualOp>;

/** Operaciones para contenedores con recorrido en ambos sentidos, con `ApplyFunction`. */
using FullTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp, RFirstThatOp,
                                      ApplyFunctionOp, PrintableOp, ReadableOp, VisualOp>;

/** Operaciones para contenedores que solo se recorren hacia adelante, sin `ApplyFunction`. */
using ForwardSearchTraversalOps = TraversalOps<CallOp, FirstThatOp, PrintableOp,
                                               ReadableOp, VisualOp>;

/** Operaciones para contenedores con recorrido en ambos sentidos, sin `ApplyFunction`. */
using SearchTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp, RFirstThatOp,
                                        PrintableOp, ReadableOp, VisualOp>;

#endif // TRAVERSALOPS_H
