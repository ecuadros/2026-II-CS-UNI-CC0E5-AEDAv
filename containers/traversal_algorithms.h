#ifndef TRAVERSAL_ALGORITHMS_H
#define TRAVERSAL_ALGORITHMS_H

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <type_traits>

namespace traversal_detail {

inline constexpr auto for_each = [](auto first, auto last, auto&& func, auto&&... args) {
    std::for_each(first, last, [&](auto& node) { std::invoke(func, node, args...); });
};

inline constexpr auto first_that = [](auto first, auto last, auto&& pred,
                                      auto&&... args) -> decltype(auto) {
    auto found = std::find_if(first, last, [&](auto& node) {
        return static_cast<bool>(std::invoke(pred, node, args...));
    });
    if (found == last) throw std::runtime_error("FirstThat: ningun elemento coincide");
    return *found;
};

inline constexpr auto legacy_call = [](auto first, auto last, auto&& func,
                                       auto&&... args) -> decltype(auto) {
    using Result = std::invoke_result_t<decltype(func)&, decltype(*first), decltype(args)&...>;
    if constexpr (std::is_void_v<Result>) {
        for_each(first, last, func, args...);
    } else {
        static_assert(std::is_convertible_v<Result, bool>, "call requiere void o un booleano");
        return first_that(first, last, func, args...);
    }
};

} // namespace traversal_detail

#endif // TRAVERSAL_ALGORITHMS_H
