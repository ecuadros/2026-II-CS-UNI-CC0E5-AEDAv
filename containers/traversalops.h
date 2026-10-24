#ifndef TRAVERSALOPS_H
#define TRAVERSALOPS_H

#include "traversal_algorithms.h"
#include <mutex>
#include <utility>

template <class Container>
concept ReverseTraversable = requires(Container& container) {
    container.rbegin();
    container.rend();
};

template <class Derived> class FirstThatOp {
public:
    template <class Pred, class... Args>
    decltype(auto) FirstThat(this Derived& c, Pred&& pred, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::first_that(c.begin(), c.end(), std::forward<Pred>(pred),
                                            std::forward<Args>(args)...);
    }
};

template <class Derived> class RFirstThatOp {
public:
    template <class Pred, class... Args>
        requires ReverseTraversable<Derived>
    decltype(auto) RFirstThat(this Derived& c, Pred&& pred, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::first_that(c.rbegin(), c.rend(), std::forward<Pred>(pred),
                                            std::forward<Args>(args)...);
    }
};

template <class Derived> class CallOp {
public:
    template <class Func, class... Args>
    decltype(auto) call(this Derived& c, Func&& func, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::legacy_call(c.begin(), c.end(), std::forward<Func>(func),
                                             std::forward<Args>(args)...);
    }
};

template <class Derived> class RCallOp {
public:
    template <class Func, class... Args>
        requires ReverseTraversable<Derived>
    decltype(auto) rcall(this Derived& c, Func&& func, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::legacy_call(c.rbegin(), c.rend(), std::forward<Func>(func),
                                             std::forward<Args>(args)...);
    }
};

template <class Derived> class ApplyFunctionOp {
public:
    template <class Func, class... Args>
    void ApplyFunction(this Derived& c, Func&& func, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        traversal_detail::for_each(c.begin(), c.end(), std::forward<Func>(func),
                                   std::forward<Args>(args)...);
    }
};

template <class Derived, template <class> class... Ops>
class TraversalOps : public Ops<Derived>... {};

template <class Derived>
using FullTraversalOps = TraversalOps<Derived, CallOp, RCallOp, FirstThatOp,
                                      RFirstThatOp, ApplyFunctionOp>;

template <class Derived>
using SearchTraversalOps = TraversalOps<Derived, CallOp, RCallOp, FirstThatOp,
                                        RFirstThatOp>;

#endif // TRAVERSALOPS_H
