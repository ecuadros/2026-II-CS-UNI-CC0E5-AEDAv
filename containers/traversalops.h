#ifndef TRAVERSALOPS_H
#define TRAVERSALOPS_H

#include "traversal_algorithms.h"
#include <mutex>
#include <utility>

template <class Derived> class FirstThatOp {
public:
    template <class Pred, class... Args> decltype(auto) FirstThat(Pred&& pred, Args&&... args) {
        auto& c = static_cast<Derived&>(*this);
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::first_that(c.begin(), c.end(), std::forward<Pred>(pred),
                                            std::forward<Args>(args)...);
    }

    template <class Pred, class... Args>
        requires requires(Derived& c) {
            c.rbegin();
            c.rend();
        }
    decltype(auto) RFirstThat(Pred&& pred, Args&&... args) {
        auto& c = static_cast<Derived&>(*this);
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::first_that(c.rbegin(), c.rend(), std::forward<Pred>(pred),
                                            std::forward<Args>(args)...);
    }
};

template <class Derived> class CallOp {
public:
    template <class Func, class... Args> decltype(auto) call(Func&& func, Args&&... args) {
        auto& c = static_cast<Derived&>(*this);
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::legacy_call(c.begin(), c.end(), std::forward<Func>(func),
                                             std::forward<Args>(args)...);
    }

    template <class Func, class... Args>
        requires requires(Derived& c) {
            c.rbegin();
            c.rend();
        }
    decltype(auto) rcall(Func&& func, Args&&... args) {
        auto& c = static_cast<Derived&>(*this);
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::legacy_call(c.rbegin(), c.rend(), std::forward<Func>(func),
                                             std::forward<Args>(args)...);
    }
};

template <class Derived> class ApplyFunctionOp {
public:
    template <class Func, class... Args> void ApplyFunction(Func&& func, Args&&... args) {
        auto& c = static_cast<Derived&>(*this);
        std::lock_guard lock(c.m_mutex);
        traversal_detail::for_each(c.begin(), c.end(), std::forward<Func>(func),
                                   std::forward<Args>(args)...);
    }
};

template <class Derived, template <class> class... ExtraOps>
class TraversalOps : public CallOp<Derived>, public ExtraOps<Derived>... {};

template <class Derived>
using FullTraversalOps = TraversalOps<Derived, FirstThatOp, ApplyFunctionOp>;

template <class Derived> using SearchTraversalOps = TraversalOps<Derived, FirstThatOp>;

#endif // TRAVERSALOPS_H
