#ifndef __GENERAL_STREAM_H__
#define __GENERAL_STREAM_H__
#include <iostream>
using namespace std;

template <typename Traits>
class GeneralContainer;

template <typename Traits>
ostream &operator<<(ostream &os, const GeneralContainer<Traits> &container) {
    return container.write(os);
}

template <typename Traits>
istream &operator>>(istream &is, GeneralContainer<Traits> &container) {
    return container.read(is);
}

#endif // __GENERAL_STREAM_H__
