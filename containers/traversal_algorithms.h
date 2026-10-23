#ifndef TRAVERSAL_ALGORITHMS_H
#define TRAVERSAL_ALGORITHMS_H

#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace traversal_detail {

template <class It, class Func, class... Args>
void for_each(It first, It last, Func&& func, Args&&... args) {
    for (; first != last; ++first)
        std::invoke(func, *first, args...);
}

template <class It, class Pred, class... Args>
It find_if(It first, It last, Pred&& pred, Args&&... args) {
    for (; first != last; ++first)
        if (std::invoke(pred, *first, args...)) return first;
    return last;
}

template <class It, class Pred, class... Args>
decltype(auto) first_that(It first, It last, Pred&& pred, Args&&... args) {
    auto found = traversal_detail::find_if(first, last, std::forward<Pred>(pred),
                                           std::forward<Args>(args)...);
    if (found == last) throw std::runtime_error("FirstThat: ningun elemento coincide");
    return *found;
}

template <class It, class Func, class... Args>
decltype(auto) legacy_call(It first, It last, Func&& func, Args&&... args) {
    using Result = std::invoke_result_t<Func&, decltype(*first), Args&...>;
    if constexpr (std::is_void_v<Result>) {
        traversal_detail::for_each(first, last, std::forward<Func>(func),
                                   std::forward<Args>(args)...);
        return;
    } else {
        static_assert(std::is_convertible_v<Result, bool>,
                      "call requiere void o un booleano");
        return traversal_detail::first_that(first, last, std::forward<Func>(func),
                                            std::forward<Args>(args)...);
    }
}

} 

#endif // TRAVERSAL_ALGORITHMS_H
