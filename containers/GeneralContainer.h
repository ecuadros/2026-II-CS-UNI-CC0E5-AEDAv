#ifndef GENERAL_CONTAINER_H
#define GENERAL_CONTAINER_H

#include "../foreach.h"
#include <utility>
#include <ostream>
#include <istream>

template <typename Derived>
class GeneralContainer {
public:
    template <typename Func, typename... Args>
    decltype(auto) call(Func func, Args&&... args) {
        auto& container = static_cast<Derived&>(*this);

        return ::call(
            container.begin(),
            container.end(),
            func,
            std::forward<Args>(args)...
        );
        }
        template <typename Func, typename... Args>
decltype(auto) rcall(Func func, Args&&... args) {
    auto& container = static_cast<Derived&>(*this);

    return ::call(
        container.rbegin(),
        container.rend(),
        func,
        std::forward<Args>(args)...
    );
    
}
template <typename Func, typename... Args>
decltype(auto) FirstThat(Func func, Args&&... args) {
    auto& container = static_cast<Derived&>(*this);

    return ::FirstThat(
        container.begin(),
        container.end(),
        func,
        std::forward<Args>(args)...
    );
}

template <typename Func, typename... Args>
void Foreach(Func func, Args&&... args) {
    auto& container = static_cast<Derived&>(*this);

    ::ApplyFunction(
        container.begin(),
        container.end(),
        func,
        std::forward<Args>(args)...
    );
}
//
friend std::ostream& operator<<(
    std::ostream& os,
    const GeneralContainer& base) {

    const auto& container = static_cast<const Derived&>(base);
    bool first = true;

    os << "[";

    auto printNode = [&os, &first](const auto& node) {
        if (!first)
            os << ",";

        os << node;
        first = false;
    };

    ::ApplyFunction(
        container.begin(),
        container.end(),
        printNode
    );

    return os << "]";
}
//
friend std::istream& operator>>(
    std::istream& is,
    GeneralContainer& base) {

    auto& container = static_cast<Derived&>(base);
    char opening{};
    char closing{};

    if (!(is >> opening) || opening != '[') {
        is.setstate(std::ios::failbit);
        return is;
    }

    if (!(is >> closing) || closing != ']') {
        is.setstate(std::ios::failbit);
        return is;
    }

    container.clear();
    return is;
}
    
};

#endif