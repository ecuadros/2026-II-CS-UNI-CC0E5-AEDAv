#ifndef __FOREACH_H__
#define __FOREACH_H__
#include <utility> // std::forward
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

template <typename Iterator, typename Func, typename... Args>
Iterator FirstThat(Iterator begin, Iterator end, Func func, Args&&... args) {
    for (auto iter = begin; iter != end; ++iter)
        if (func(*iter, args...))
            return iter;
    return end;
}

#endif // __FOREACH_H__