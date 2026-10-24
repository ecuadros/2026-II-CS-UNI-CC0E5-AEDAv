#ifndef __GENERAL_CONTAINER_H__
#define __GENERAL_CONTAINER_H__
#include <mutex>
#include <vector>
#include <ranges>
#include "../foreach.h"

template <typename Derived>
class GeneralContainer {
protected:
    mutable mutex m_mutex;
    void AfterApply() {}
private:
    Derived& self() { return static_cast<Derived&>(*this); }
public:
    mutex& GetMutex() const { return m_mutex; }

    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args... args) {
        lock_guard<mutex> lock(m_mutex);
        ::call(self().begin(), self().end(), func, std::forward<Args>(args)...);
        self().AfterApply();
    }

    template <typename Func, typename... Args>
    decltype(auto) FirstThat(Func func, Args... args) {
        return call(func, std::forward<Args>(args)...);
    }

    template <typename Func, typename... Args>
    decltype(auto) call(Func func, Args&&... args) {
        lock_guard<mutex> lock(m_mutex);
        return ::call(self().begin(), self().end(), func, std::forward<Args>(args)...);
    }

    template <typename Func, typename... Args>
    decltype(auto) rcall(Func func, Args&&... args) {
        lock_guard<mutex> lock(m_mutex);
        if constexpr (requires { self().rbegin(); })
            return ::call(self().rbegin(), self().rend(), func, std::forward<Args>(args)...);
        else {
            using Node = typename Derived::Node;
            vector<Node*> nodes;
            for (auto &node : self())
                nodes.push_back(&node);
            auto reversed = nodes | views::reverse | views::transform([](Node *p) -> Node& { return *p; });
            return ::call(reversed.begin(), reversed.end(), func, std::forward<Args>(args)...);
        }
    }
};

#endif // __GENERAL_CONTAINER_H__
