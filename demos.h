/**
 * @file demos.h
 * @ingroup demos
 * @brief Declaraciones de las funciones de demostración de estructuras de datos y utilidades.
 */

#ifndef __DEMOS_H__
#define __DEMOS_H__
#include "types.h"
#include "containers/trees/heap.h"

/** @brief Demostración de operaciones y renderizado de `Vector`. */
void DemoVector();
/** @brief Demostración de contenedores personalizados y serialización. */
void DemoCustomContainer();
/** @brief Demostración de condición de carrera en operaciones concurrentes. */
void DemoRaceCondition();
/** @brief Demostración de listas enlazadas (simple, doble, circular) y su renderizado. */
void DemoLinkedList();
/** @brief Demostración de montículos (`Heap` / `MinHeap` / `MaxHeap`) y `rebuild()`. */
void DemoHeap();
/** @brief Demostración de árboles binarios, recorridos de Euler y `AVLTree`. */
void DemoBinaryTree();
/** @brief Demostración de tablas hash (`HashTable`) con encadenamiento. */
void DemoHashTable();
/** @brief Demostración de árboles de Merkle (`MerkleTree`) y pruebas de auditoría. */
void DemoMerkleTree();
/** @brief Demostración de posicionamiento directo en consola con `gotoxy`. */
void DemoGotoxy();

/** @brief Demostración de la API y del balanceo de `BTree`. */
void DemoBTree();
/** @brief Demostración de la API y del balanceo de `BStarTree`. */
void DemoBStarTree();
/** @brief Comparación polimórfica `BTree` vs `BStarTree` y tabla de ocupación con 10 000 claves. */
void DemoBTreeComparison();
/** @brief Demostración de inserciones concurrentes en un `BStarTree` con verificación de invariantes. */
void DemoBTreeConcurrency();

#endif // __DEMOS_H__