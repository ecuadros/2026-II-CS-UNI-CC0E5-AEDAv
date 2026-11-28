/**
 * @file bstar_btree_test.cpp
 * @ingroup trees
 * @brief Suite de `btree_suite.h` para el árbol B* (`BStarTree`), órdenes 4..16.
 */

#include "btree_suite.h"

TX main() { return runSuite<BStarTree>("BStarTree", {4, 5, 6, 7, 8, 16}); }
