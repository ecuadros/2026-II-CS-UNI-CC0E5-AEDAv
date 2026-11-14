#include "Demos.h"
#include <iostream>
#include "containers/hashtable.h"
#include <string_view>

// Compilar asi: make
// Ejecutar asi: ./main avl hash   (sin argumentos corre todos)
/**
 * Ejecuta las demos indicadas por nombre en los argumentos, o todas si no se pasa ninguno.
 * @return 0 si todo fue bien, 1 si algún nombre de demo no existe.
 */
int main(int argc, char *argv[]) {
  const HashTable<std::string_view, void (*)()> demos{
      {"vector", DemoVector},
      {"race", DemoRaceCondition},
      {"list", DemoLinkedList},
      {"circular", DemoCircularLinkedList},
      {"double", DemoDoubleLinkedList},
      {"circular-double", DemoCircularDoubleLinkedList},
      {"heap", DemoHeap},
      {"bst", DemoBinaryTree},
      {"avl", DemoAVL},
      {"hash", DemoHashTable},
      {"avl-steps", DemoAVLStepByStep},
      {"hash-steps", DemoHashTableStepByStep},
      {"merkle-base", DemoMerkleTree},
      {"merkle-bitcoin", DemoMerkleBitcoin},
      {"merkle-steps", DemoMerkleTreeStepByStep},
      {"canvas", DemoCanvas},
  };

  if (argc == 1) {
    for (const auto &[name, demo] : demos)
      demo();
    return 0;
  }

  for (int i = 1; i < argc; ++i) {
    auto it = demos.find(argv[i]);
    if (it == demos.end()) {
      std::cerr << "Demo desconocido: " << argv[i] << "\nDisponibles:";
      for (const auto &[name, demo] : demos)
        std::cerr << ' ' << name;
      std::cerr << '\n';
      return 1;
    }
    it->second();
  }
  return 0;
}
