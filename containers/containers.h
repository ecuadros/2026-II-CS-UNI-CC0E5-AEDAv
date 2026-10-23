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
class Applicable: public Callable<Applicable<Derived, Traits>, Traits> {
protected:
    /*virtual typename Derived::ForwardIterator begin() = 0;
    virtual typename Derived::ForwardIterator end() = 0;*/
public:
    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args... args) {
        this->call(func, std::forward<Args>(args)...);
    }
};

template <typename Derived, typename Traits>
class Firstable: public Callable<Firstable<Derived, Traits>, Traits> {
protected:
    /*virtual typename Derived::ForwardIterator begin() = 0;
        virtual typename Derived::ForwardIterator end() = 0;*/
public:
    template <typename Func, typename... Args>
    typename Traits::Node& FirstThat(Func func, Args... args) {
        return this->call(func, std::forward<Args>(args)...);
    }
};

#endif // __CONTAINERS_H__
