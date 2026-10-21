#ifndef __PERSISTENCE_H__
#define __PERSISTENCE_H__
#include <iostream>
//#include "GeneralNode.h"

template<typename Container>
std::ostream& write(std::ostream& os, const Container &container) { return os << container; }

template<typename Container>
std::ostream &operator <<(std::ostream &os, const Container &container) {
    os << "[";
    for (auto it = container.begin(); it != container.end(); ++it) {
        os << *it;
        if (std::next(it) != container.end()) {
            os << ", ";
        }
    }
    return os << "]";
}

#endif // __PERSISTENCE_H__
