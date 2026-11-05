#include "Demos.h"
#include <iostream>
#include <map>
#include <string_view>

// Compilar asi: make
// Ejecutar asi: ./main avl hash   (sin argumentos corre todos)
int main(int argc, char *argv[]) {
  const std::map<std::string_view, void (*)()> demos{
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
