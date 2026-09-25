#ifndef __FOREACH_H__
#define __FOREACH_H__
#include <utility> // std::forward
#include <type_traits>
#include <functional>
using namespace std;

// Variadic template to allow passing additional arguments to the function
// template <typename Container, typename Func, typename... Args>
// Iterator Level #1
// void ApplyFunction(Container &container, Func func, Args... args) {
//     for (size_t i = 0; i < container.size(); ++i) {
//         func(container[i], args...);
//     }
// }

// Iterator Level #2
// template <typename Container, typename Func, typename... Args>
// void ApplyFunction(Container &container, Func func, Args... args) {
//     for (auto iter = container.begin(); iter != container.end(); ++iter) {
//         func(*iter, args...);
//     }
// }

// Iterator Level #3
// args se reenvia en cada iteracion del for: si Args deduce un rvalue real
// (no una referencia), forward lo moveria repetidamente en cada llamada a
// func, dejandolo invalido despues de la primera. Usar args solo para
// referencias/constantes compartidas entre elementos (streams, valores a
// sumar, etc.), no para recursos que func deba consumir/mover.
template <typename Iterator, typename Func, typename... Args>
void ApplyFunction(Iterator begin, Iterator end, Func func, Args&&... args) {
    for (auto iter = begin; iter != end; ++iter)
        func(*iter, std::forward<Args>(args)...);
}

// // Iterator Level #3
// template <typename Container, typename Func, typename... Args>
// void ApplyFunction(Container &container, Func func, Args... args) {
//     ::ApplyFunction(container.begin(), container.end(), func, args...);
// }

// Iterator Level #4
template <typename Container, typename Func, typename... Args>
void ApplyFunction(Container &container, Func func, Args&&... args) {
    for (auto &v : container)
        func(v, std::forward<Args>(args)...);
}

template <typename Iterator, typename Predicate, typename... Args>
Iterator FirstThat(Iterator begin, Iterator end, Predicate pred, Args&&... args) {
    for (auto iter = begin; iter != end; ++iter)
        if (pred(*iter, std::forward<Args>(args)...))
            return iter;
    return end;
}

template <typename Func, typename... Args>
decltype(auto) call(Func&& func, Args&&... args) {
    if constexpr (std::is_void_v<std::invoke_result_t<Func, Args...>>) {
        std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
        return;
    } else {
        return std::invoke(std::forward<Func>(func), std::forward<Args>(args)...);
    }
}
#endif // __FOREACH_H__