#ifndef __CONTAINERS_H__
#define __CONTAINERS_H__
#include <complex>
#include <mutex>
#include "../foreach.h"

template <typename Derived, typename Traits>
class Callable {
protected:
    mutable std::mutex m_mutex; // mutex para sincronización

    virtual typename Traits::ForwardIterator begin() = 0;
    virtual typename Traits::ForwardIterator end() = 0;

public:
    template<typename Func, typename... Args>
    decltype(auto) call(Func func, Args&&... args)
    {    lock_guard<mutex> lock(m_mutex);
        if constexpr(is_void_v<invoke_result_t<Func, typename Derived::Node&, Args...>>)
            ::call(begin(), end(), std::forward<Func>(func), std::forward<Args>(args)...);
        else // return type is not void:
            return ::call(begin(), end(), std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

template <typename Derived, typename Traits>
class Applicable: public virtual Callable<Derived, Traits> {
public:
    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args... args) {
        this->call(func, std::forward<Args>(args)...);
    }
};

template <typename Derived, typename Traits>
class Firstable: public virtual Callable<Derived, Traits> {
public:
    template <typename Func, typename... Args>
    typename Traits::Node& FirstThat(Func func, Args... args) {
        return this->call(func, std::forward<Args>(args)...);
    }
};

template <typename Derived, typename Traits>
class Recallable: public virtual Callable<Derived, Traits> {
protected:
    virtual typename Traits::BackwardIterator rbegin() = 0;
    virtual typename Traits::BackwardIterator rend() = 0;
public:
    template<typename Func, typename... Args>
    decltype(auto) rcall(Func func, Args&&... args)
    {    lock_guard<mutex> lock(this->m_mutex);
        if constexpr(is_void_v<invoke_result_t<Func, typename Derived::Node&, Args...>>)
            ::call(rbegin(), rend(), std::forward<Func>(func), std::forward<Args>(args)...);
        else // return type is not void:
            return ::call(rbegin(), rend(), std::forward<Func>(func), std::forward<Args>(args)...);
    }
};

#endif // __CONTAINERS_H__
