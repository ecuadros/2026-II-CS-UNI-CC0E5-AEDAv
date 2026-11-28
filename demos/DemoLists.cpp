/** @file
 * Demos de Vector y de las listas enlazadas.
 */

#include "utils/generic_tests.h"

void DemoRaceCondition() { DemoRaceCondition<VectorOf<TX>>("Vector"); }

void DemoVector() {
    VectorOf<TX> vec;
    Demo(vec, "Vector");

    VectorOf<std::string> strVec;
    TestContainer(strVec, {{"Hello", 1}, {"World", 2}}, "vector_str.txt");
    TestTraversal(strVec, "Vector<string>");

    const auto& constVec = vec;
    CheckReverse("Vector const", constVec, ReverseRange(constVec));
    if (CollectRefs(constVec).size() != vec.size())
        throw std::runtime_error("Vector const: recorridos inconsistentes");

    VectorOf<TX> emptyVector;
    if (emptyVector.rbegin() != emptyVector.rend())
        throw std::runtime_error("Vector vacio: recorrido inverso incorrecto");
    const auto& constEmpty = emptyVector;
    if (constEmpty.begin() != constEmpty.end() || constEmpty.rbegin() != constEmpty.rend())
        throw std::runtime_error("Vector const vacio: recorrido incorrecto");
    std::cout << "Vector: recorrido const y vacio OK\n";
}

void DemoLinkedList() { Demo<SortedLinkedList<TX>, SortedLinkedList<TX, std::greater<TX>>>("LinkedList"); }

void DemoCircularLinkedList() {
    Demo<SortedCircularLinkedList<TX>, SortedCircularLinkedList<TX, std::greater<TX>>>(
        "CircularLinkedList");
}

void DemoDoubleLinkedList() {
    Demo<SortedDoubleLinkedList<TX>, SortedDoubleLinkedList<TX, std::greater<TX>>>("DoubleLinkedList");
}

void DemoCircularDoubleLinkedList() {
    Demo<SortedCircularDoubleLinkedList<TX>,
         SortedCircularDoubleLinkedList<TX, std::greater<TX>>>(
        "CircularDoubleLinkedList");
}
