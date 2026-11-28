/** @file
 * Demos de MinHeap y MaxHeap.
 */

#include "utils/generic_tests.h"

void DemoHeap() {
    Demo<MinHeap<Int>>("MinHeap");
    Demo<MaxHeap<Int>>("MaxHeap");
}
