#include "types.h"
#include "Demos.h"
#include "containers/visual/Tui.h"
#include <iostream>
#include "containers/aliases.h"
#include <string_view>

// Compilar asi: make
// Ejecutar asi: ./main avl hash   (sin argumentos corre todos)
// Interfaz TUI: ./main tui
/**
 * Ejecuta las demos indicadas por nombre en los argumentos, o todas si no se pasa ninguno.
 * @return 0 si todo fue bien, 1 si algún nombre de demo no existe.
 */
Int main(Int argc, Char *argv[]) {
  if (argc >= 2 && std::string_view(argv[1]) == "tui") {
    RunTUI();
    return 0;
  }

  const HashTableOf<std::string_view, void (*)()> demos{
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
      {"btree", DemoBTree},
      {"bplus", DemoBPlusTree},
      {"graph", DemoGraph},
      {"rtree", DemoRTree},
      {"files", DemoFiles},
      {"regresiones", DemoRegresiones},
      {"canvas", DemoCanvas},
      {"sheet", DemoSheet},
      {"sheet-steps", DemoSheetStepByStep},
  };

  if (argc == 1) {
    for (const auto &[name, demo] : demos)
      demo();
    return 0;
  }

  for (Int i = 1; i < argc; ++i) {
    auto it = demos.find(argv[i]);
    if (it == demos.end()) {
      std::cerr << "Demo desconocido: " << argv[i] << "\nDisponibles: tui";
      for (const auto &[name, demo] : demos)
        std::cerr << ' ' << name;
      std::cerr << '\n';
      return 1;
    }
    it->second();
  }
  return 0;
}
