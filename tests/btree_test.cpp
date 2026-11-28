/**
 * @file btree_test.cpp
 * @ingroup trees
 * @brief Suite de `btree_suite.h` para el árbol B clásico (`BTree`), órdenes 3..16.
 */

#include "btree_suite.h"

TX main() { return runSuite<BTree>("BTree", {3, 4, 5, 6, 7, 16}); }
