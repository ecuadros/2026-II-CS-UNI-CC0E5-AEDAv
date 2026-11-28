/** @file
 * Junta todas las demos en una sola unidad de compilación (unity build): cada estructura tiene su archivo
 * en `demos/` y los ayudantes compartidos están en `demos/utils/`. Así se sigue compilando con
 * `make` (o `g++ main.cpp Demos.cpp containers/visual/Tui.cpp`) sin agregar archivos al comando.
 */
#include "demos/DemoLists.cpp"
#include "demos/DemoHeap.cpp"
#include "demos/DemoTrees.cpp"
#include "demos/DemoHash.cpp"
#include "demos/DemoBTree.cpp"
#include "demos/DemoMerkle.cpp"
#include "demos/DemoSheet.cpp"
#include "demos/DemoFiles.cpp"
