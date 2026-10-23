#ifndef __GENERAL_CONTAINER_H__
#define __GENERAL_CONTAINER_H__
#include <iostream>
#include <mutex>
#include "GeneralTraits.h"
#include "../foreach.h"
#include "../GeneralStream.h" 
#include "../types.h" 
using namespace std;

template <typename Traits>
class GeneralContainer {
public:
    using value_type      = typename Traits::value_type;
    using Node            = typename Traits::Node;
    using ForwardIterator = typename Traits::ForwardIterator;
protected:
    mutable mutex m_mutex;   

public:
    GeneralContainer() = default;
    GeneralContainer(const GeneralContainer &) {}
    GeneralContainer &operator=(const GeneralContainer &) { return *this; }
    virtual ~GeneralContainer() = default;   

    virtual ForwardIterator begin() const = 0;
    virtual ForwardIterator end()   const = 0;
    virtual void push_back(const value_type &value, Ref ref) = 0;
    virtual void clear() = 0;

    template <typename Func, typename... Args>
    decltype(auto) call(Func func, Args&&... args) {
        lock_guard<mutex> lock(m_mutex);
        return ::call(begin(), end(), func, std::forward<Args>(args)...);
    }
    template <typename Func, typename... Args>
    void ApplyFunction(Func func, Args&&... args) {
        call(func, std::forward<Args>(args)...);
    }
    template <typename Func, typename... Args>
    Node &FirstThat(Func func, Args&&... args) {
        return call(func, std::forward<Args>(args)...);
    }

    ostream &write(ostream &os) const {
        lock_guard<mutex> lock(m_mutex);
        os << "[";
        bool first = true;
        for (auto it = begin(); it != end(); ++it) {
            if (!first) os << ",";
            os << *it;
            first = false;
        }
        return os << "]";
    }
    istream &read(istream &is) {    
        clear();
        char d;
        if (!(is >> d) || d != '[') {
            is.setstate(ios::failbit);
            return is;
        }
        if ((is >> ws).peek() == ']') {     
            is.get();
            return is;
        }
        Node node;
        while (is >> node) {
            push_back(node.getValue(), node.getRef());
            if (!(is >> d) || d == ']')
                break;
            if (d != ',') {
                is.setstate(ios::failbit);
                break;
            }
        }
        return is;
    }
};

template <typename Traits>
class ReversibleContainer : virtual public GeneralContainer<Traits> {
public:
    using BackwardIterator = typename Traits::BackwardIterator;

    virtual BackwardIterator rbegin() const = 0;
    virtual BackwardIterator rend()   const = 0;

    template <typename Func, typename... Args>
    decltype(auto) rcall(Func func, Args&&... args) {
        lock_guard<mutex> lock(this->m_mutex);   // base dependiente: this-> obligatorio
        return ::call(rbegin(), rend(), func, std::forward<Args>(args)...);
    }
};

#endif // __GENERAL_CONTAINER_H__
