#ifndef TRAVERSALOPS_H
#define TRAVERSALOPS_H

#include "../foreach.h"
#include <mutex>
#include <utility>

template <class Container> class TraversalOps {
public:
  template <class Func, class... Args>
  decltype(auto) call(Func func, Args &&...args) {
    Container &container = static_cast<Container &>(*this);
    std::lock_guard<std::mutex> lock(container.m_mutex);
    return ::call(container.begin(), container.end(), std::forward<Func>(func),
                  std::forward<Args>(args)...);
  }

  template <class Func, class... Args>
    requires requires { typename Container::traits_type::BackwardIterator; }
  decltype(auto) rcall(Func func, Args &&...args) {
    Container &container = static_cast<Container &>(*this);
    std::lock_guard<std::mutex> lock(container.m_mutex);
    using Iterator = typename Container::traits_type::BackwardIterator;
    return ::call(Iterator(container.m_pTail), Iterator(nullptr),
                  std::forward<Func>(func), std::forward<Args>(args)...);
  }

  template <class Func, class... Args>
  void ApplyFunction(Func func, Args &&...args) {
    call(std::forward<Func>(func), std::forward<Args>(args)...);
  }

  template <class Func, class... Args>
  decltype(auto) FirstThat(Func func, Args &&...args) {
    return call(std::forward<Func>(func), std::forward<Args>(args)...);
  }
};

#endif // TRAVERSALOPS_H
