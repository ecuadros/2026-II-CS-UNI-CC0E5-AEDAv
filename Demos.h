#ifndef __DEMOS_H__
#define __DEMOS_H__

/** Demo de Vector: inserción, recorridos, lectura y escritura, copia y dibujo. */
void DemoVector();
/** Demo de acceso concurrente a un Vector desde varios hilos. */
void DemoRaceCondition();
/** Demo de la lista simplemente enlazada ordenada. */
void DemoLinkedList();
/** Demo de la lista circular simplemente enlazada. */
void DemoCircularLinkedList();
/** Demo de la lista doblemente enlazada. */
void DemoDoubleLinkedList();
/** Demo de la lista circular doblemente enlazada. */
void DemoCircularDoubleLinkedList();
/** Demo de MinHeap y MaxHeap. */
void DemoHeap();
/** Demo del árbol binario de búsqueda. */
void DemoBinaryTree();
/** Demo del árbol AVL. */
void DemoAVL();
/** Demo de HashTable con interfaz de mapa. */
void DemoHashTable();
/** Demo del AVL que dibuja el árbol tras cada inserción y eliminación. */
void DemoAVLStepByStep();
/** Demo de HashTable que dibuja su árbol interno tras cada operación. */
void DemoHashTableStepByStep();
/** Demo de MerkleTree: construcción, pruebas de inclusión, actualización y copia. */
void DemoMerkleTree();
/** Demo de MerkleTree con el hasher de Bitcoin; comprueba la raíz del bloque 170. */
void DemoMerkleBitcoin();
/** Demo de MerkleTree que muestra el árbol paso a paso. */
void DemoMerkleTreeStepByStep();

#endif // __DEMOS_H__
