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

class FirstThatOp {
public:
    template <class Self, class Pred, class... Args>
    decltype(auto) FirstThat(this Self& c, Pred&& pred, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::first_that(c.begin(), c.end(), std::forward<Pred>(pred),
                                            std::forward<Args>(args)...);
    }
};

class RFirstThatOp {
public:
    template <class Self, class Pred, class... Args>
        requires ReverseTraversable<Self>
    decltype(auto) RFirstThat(this Self& c, Pred&& pred, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::first_that(c.rbegin(), c.rend(), std::forward<Pred>(pred),
                                            std::forward<Args>(args)...);
    }
};

class CallOp {
public:
    template <class Self, class Func, class... Args>
    decltype(auto) call(this Self& c, Func&& func, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::legacy_call(c.begin(), c.end(), std::forward<Func>(func),
                                             std::forward<Args>(args)...);
    }
};

class RCallOp {
public:
    template <class Self, class Func, class... Args>
        requires ReverseTraversable<Self>
    decltype(auto) rcall(this Self& c, Func&& func, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        return traversal_detail::legacy_call(c.rbegin(), c.rend(), std::forward<Func>(func),
                                             std::forward<Args>(args)...);
    }
};

class ApplyFunctionOp {
public:
    template <class Self, class Func, class... Args>
    void ApplyFunction(this Self& c, Func&& func, Args&&... args) {
        std::lock_guard lock(c.m_mutex);
        traversal_detail::for_each(c.begin(), c.end(), std::forward<Func>(func),
                                   std::forward<Args>(args)...);
    }
};

template <class... Ops>
class TraversalOps : public Ops... {};

using FullTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp,
                                      RFirstThatOp, ApplyFunctionOp>;

using SearchTraversalOps = TraversalOps<CallOp, RCallOp, FirstThatOp,
                                        RFirstThatOp>;

#endif // TRAVERSALOPS_H
