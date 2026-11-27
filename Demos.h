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
/** Demo del árbol B*: pruebas genéricas e inserción, búsqueda y eliminación de caracteres. */
void DemoBTree();
/** Demo del árbol B+: pruebas genéricas, rangos, operaciones aleatorias, persistencia y concurrencia. */
void DemoBPlusTree();
/** Demo del grafo: recorridos, A*, persistencia, concurrencia y exportación VTK. */
void DemoGraph();
/** Demo del árbol R: inserción, búsqueda, eliminación, persistencia, concurrencia y exportación VTK. */
void DemoRTree();
/** Demo de lectura y escritura de archivos en todos los contenedores, con el formato `id valor`. */
void DemoFiles();
/** Comprobaciones de regresión: reentrada en `call`, iteradores inversos de un Vector vacío y liberación en `pop_back`. */
void DemoRegresiones();
/** Demo del lienzo de visualización: escribe texto en posiciones (x, y). */
void DemoCanvas();
/** Hoja de cálculo de texto: fórmulas, funciones, errores, archivo e hilos. */
void DemoSheet();
/** Hoja de cálculo paso a paso: cada set y su tabla, cascada, errores y lectura/escritura. */
void DemoSheetStepByStep();

#endif // __DEMOS_H__
