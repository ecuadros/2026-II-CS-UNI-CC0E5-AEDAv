/** @file
 * Demos del árbol binario de búsqueda y del AVL, incluida la versión paso a paso.
 */

#include "utils/avl_steps.h"

void DemoBinaryTree() { Demo<BinarySearchTree<TX>, BinarySearchTree<TX, std::greater<TX>>>("BinaryTree"); }

void DemoAVL() {
    Demo<AVLSearchTree<TX>, AVLSearchTree<TX, std::greater<TX>>>("AVL");
    DemoRaceCondition<AVLSearchTree<TX>>("AVL");
}

void DemoAVLStepByStep() {
    PrintColor("DemoAVL paso a paso");
    AVLSearchTree<TX> tree;
    for (TX v : {10, 20, 30, 40, 50, 25, 5, 4, 8, 7})
        AVLStep(tree, "insert(" + std::to_string(v) + ")", [v](auto& t, auto& s) {
            s.insert(v, v * 10);
            t.insert(v, v * 10);
        });
    for (TX v : {50, 40, 4, 30})
        AVLStep(tree, "remove(" + std::to_string(v) + ")", [v](auto& t, auto& s) {
            s.remove(v);
            t.remove(v);
        });
    CheckTree(tree, "AVL paso a paso");
}
