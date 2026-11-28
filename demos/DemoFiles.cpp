/** @file
 * Demos de regresiones y de lectura/escritura de archivos en todos los contenedores.
 */

#include "utils/generic_tests.h"

void DemoRegresiones() {
    PrintColor("DemoRegresiones");
    VectorOf<TX> empty;
    Check(empty.rbegin() == empty.rend(), "Regresiones", "un Vector vacio debe tener rbegin() == rend()");

    VectorOf<TX> v;
    v.push_back(1, 0);
    v.push_back(2, 1);
    std::size_t seen = 0;
    v.call([&](const auto&) { seen += v.size(); });
    Check(seen == 4, "Regresiones", "call debe poder consultar el mismo Vector");

    VectorOf<std::string> words;
    words.push_back(std::string(100, 'x'), 0);
    words.pop_back();
    words.push_back("a", 1);
    Check(words.at(0).getValue() == "a" && words.size() == 1, "Regresiones", "pop_back y push_back fallaron");
    std::cout << "Regresiones: OK\n";
}

/** Guarda y lee @p container con los dos formatos de archivo y comprueba que el contenido no cambia. */
template <typename Container> void TestFileRoundTrip(const std::string& name, const Values<Container>& values) {
    Container original;
    for (const auto& [value, ref] : values)
        original.push_back(value, ref);
    const std::string bracket = "files_" + name + ".txt", records = "files_" + name + ".rec";
    original.save(bracket);
    original.save_records(records);
    Container fromBracket, fromRecords;
    fromBracket.load(bracket);
    std::size_t count = fromRecords.load_records(records);
    std::remove(bracket.c_str());
    std::remove(records.c_str());
    Expect(Sorted(original) == Sorted(fromBracket), name + ": save/load cambio el contenido");
    Expect(Sorted(original) == Sorted(fromRecords) && count == values.size(), name + ": save_records/load_records cambio el contenido");
    std::cout << "  " << name << ": " << count << " elementos por save/load y save_records/load_records OK\n";
}

/** Escribe @p text en @p path. */
static void WriteText(const std::string& path, const std::string& text) {
    std::ofstream(path, std::ios::trunc) << text;
}

void DemoFiles() {
    PrintColor("DemoFiles");
    const Values<VectorOf<TX>> numbers{{5, 50}, {1, 10}, {4, 40}, {2, 20}, {7, 70}};
    TestFileRoundTrip<VectorOf<TX>>("Vector", numbers);
    TestFileRoundTrip<SortedLinkedList<TX>>("LinkedList", numbers);
    TestFileRoundTrip<SortedCircularLinkedList<TX>>("CircularLinkedList", numbers);
    TestFileRoundTrip<SortedDoubleLinkedList<TX>>("DoubleLinkedList", numbers);
    TestFileRoundTrip<SortedCircularDoubleLinkedList<TX>>("CircularDoubleLinkedList", numbers);
    TestFileRoundTrip<MinHeap<TX>>("Heap", numbers);
    TestFileRoundTrip<BinarySearchTree<TX>>("BinaryTree", numbers);
    TestFileRoundTrip<AVLSearchTree<TX>>("AVL", numbers);
    TestFileRoundTrip<BTreeOf<TX>>("BTree", numbers);
    TestFileRoundTrip<VectorOf<std::string>>("VectorStrings", {{"hola mundo", 1}, {"a,(b)", 2}, {"[x]", 3}, {"", 4}});

    PrintColor("DemoFiles (archivo escrito a mano con identificadores)");
    const std::string path = "files_manual.txt";
    WriteText(path, "# id valor\n101 hola mundo\n\n102 segunda linea\r\n  103   con espacios\n");
    VectorOf<std::string> words;
    Expect(words.load_records(path) == 3, "Files: cantidad de registros");
    for (std::size_t i = 0; i < words.size(); ++i)
        std::cout << "  id " << words[i].getRef() << " -> \"" << words[i].getValue() << "\"\n";
    Expect(words[0].getRef() == 101 && words[0].getValue() == "hola mundo" && words[1].getValue() == "segunda linea" &&
               words[2].getRef() == 103 && words[2].getValue() == "con espacios",
           "Files: registros mal leidos");

    AVLSearchTree<TX> tree;
    WriteText(path, "30 3\n10 1\n20 2\n");
    tree.load_records(path);
    Expect(Sorted(tree) == ArrayOf<std::pair<TX, Ref>>{{1, 10}, {2, 20}, {3, 30}}, "Files: arbol desde registros");

    const auto before = Sorted(tree);
    for (const char* bad : {"30 3\nxx 5\n", "30\n", "30 abc\n", "30 3 extra\n40\n"}) {
        WriteText(path, bad);
        Bool rejected = false;
        try {
            tree.load_records(path);
        } catch (const std::runtime_error& error) {
            rejected = true;
            std::cout << "  rechazado: " << error.what() << '\n';
        }
        Expect(rejected && Sorted(tree) == before, std::string("Files: registro invalido mal manejado: ") + bad);
    }
    Bool rejected = false;
    try {
        tree.load_records("no_existe.rec");
    } catch (const std::runtime_error& error) {
        rejected = true;
        std::cout << "  rechazado: " << error.what() << '\n';
    }
    Expect(rejected, "Files: abrio un archivo inexistente");
    rejected = false;
    try {
        tree.load("no_existe.txt");
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    Expect(rejected, "Files: load abrio un archivo inexistente");
    std::remove(path.c_str());
    std::cout << "Files: OK\n";
}
