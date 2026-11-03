#ifndef TRAVERSALOPS_H
#define TRAVERSALOPS_H

#include "container_stream.h"
#include "traversal_algorithms.h"
#include "visualization.h"
#include <istream>
#include <mutex>
#include <utility>

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

template <class Container>
concept ReverseTraversable = requires(Container& container) {
    container.rbegin();
    container.rend();
};

class LockableOp {
    friend struct LockedTraversal;

protected:
    mutable std::mutex m_mutex;
};

struct LockedTraversal {
    template <class Self>
    static std::lock_guard<std::mutex> guard(Self& c) {
        return std::lock_guard<std::mutex>(c.m_mutex);
    }

    template <bool Reverse, class Self, class Algorithm, class... Args>
    static decltype(auto) run(Self& c, Algorithm algorithm, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        if constexpr (Reverse)
            return algorithm(c.rbegin(), c.rend(), std::forward<Args>(args)...);
        else
            return algorithm(c.begin(), c.end(), std::forward<Args>(args)...);
    }
};

class FirstThatOp {
public:
    template <class Self, class Pred, class... Args>
    decltype(auto) FirstThat(this Self& c, Pred&& pred, Args&&... args) {
        return LockedTraversal::run<false>(c, traversal_detail::first_that_fn,
                                           std::forward<Pred>(pred), std::forward<Args>(args)...);
    }
};

class RFirstThatOp {
public:
    template <class Self, class Pred, class... Args>
        requires ReverseTraversable<Self>
    decltype(auto) RFirstThat(this Self& c, Pred&& pred, Args&&... args) {
        return LockedTraversal::run<true>(c, traversal_detail::first_that_fn,
                                          std::forward<Pred>(pred), std::forward<Args>(args)...);
    }
};

class CallOp {
public:
    template <class Self, class Func, class... Args>
    decltype(auto) call(this Self& c, Func&& func, Args&&... args) {
        return LockedTraversal::run<false>(c, traversal_detail::legacy_call_fn,
                                           std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

class RCallOp {
public:
    template <class Self, class Func, class... Args>
        requires ReverseTraversable<Self>
    decltype(auto) rcall(this Self& c, Func&& func, Args&&... args) {
        return LockedTraversal::run<true>(c, traversal_detail::legacy_call_fn,
                                          std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

class ApplyFunctionOp {
public:
    template <class Self, class Func, class... Args>
    void ApplyFunction(this Self& c, Func&& func, Args&&... args) {
        LockedTraversal::run<false>(c, traversal_detail::for_each_fn,
                                    std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

class PrintableOp {
    template <container_stream::PrintableContainer Container>
    friend std::ostream& operator<<(std::ostream& os, const Container& container) {
        return container_stream::write(os, container);
    }
};

class ReadableOp {
    template <container_stream::ReadableContainer Container>
    friend std::istream& operator>>(std::istream& is, Container& container) {
        return container_stream::read(is, container);
    }
};

class VisualOp {
public:
    template <class Self>
        requires visual::HasTreeView<Self>
    void print_sideways(this const Self& self, std::ostream& os) {
        auto lock = LockedTraversal::guard(self);
        visual::print_sideways(os, self.tree_view());
    }

    template <class Self>
        requires visual::HasTreeView<Self>
    void print_tree(this const Self& self, std::ostream& os) {
        auto lock = LockedTraversal::guard(self);
        visual::print_tree(os, self.tree_view());
    }

    template <class Self>
        requires visual::HasTreeView<Self>
    void print_cursor(this const Self& self, std::ostream& os) {
        auto lock = LockedTraversal::guard(self);
        visual::print_cursor(os, self.tree_view());
    }

    template <class Self>
        requires visual::LinearView<Self>
    void print_linear(this const Self& self, std::ostream& os) {
        auto lock = LockedTraversal::guard(self);
        visual::print_linear(os, self);
    }

    template <class Self>
        requires visual::HasTreeView<Self> || visual::LinearView<Self>
    void visualize(this const Self& self, std::ostream& os) {
        if constexpr (visual::HasTreeView<Self>)
            self.print_tree(os);
        else
            self.print_linear(os);
    }
};

template <class... Ops>
class TraversalOps : public LockableOp, public Ops... {};

using FullTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp,
                                      RFirstThatOp, ApplyFunctionOp,
                                      PrintableOp, ReadableOp, VisualOp>;

using SearchTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp,
                                         RFirstThatOp, PrintableOp,
                                         ReadableOp, VisualOp>;

#endif // TRAVERSALOPS_H
