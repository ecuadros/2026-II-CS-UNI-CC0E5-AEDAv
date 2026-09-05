
#include <iostream>
#include "containers/vector.h"
#include "Demos.h"
using namespace std;

void DemoVector() {
    Vector vec;
    for (unsigned long long i = 0; i < 10; ++i) {
        vec.push_back(i);
    }

    cout << "Vector contents: ";
    for (int i = 0; i < vec.size(); ++i) {
        cout << vec[i] << " ";
    }
    cout << endl;
}