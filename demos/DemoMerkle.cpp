/** @file
 * Demos del árbol de Merkle: base, bloques de Bitcoin y versión paso a paso.
 */

#include "utils/generic_tests.h"

/** Árbol de Merkle: raíz, pruebas de inclusión, verificación, `update`, `push_back` y lectura/escritura. */
template <typename M>
concept MerkleLike = requires(M& m, const M& cm, std::size_t index, const typename M::value_type& block,
                              const typename M::value_type* first, const typename M::Digest& root, std::ostream& os,
                              std::istream& is) {
    os << cm;
    is >> m;
    requires std::equality_comparable<typename M::Digest>;
    requires std::copy_constructible<M>;
    M(first, first);
    { cm.root_hash() } -> std::same_as<typename M::Digest>;
    { cm.proof(index) } -> std::same_as<typename M::Proof>;
    { M::verify(block, cm.proof(index), root) } -> std::same_as<Bool>;
    m.update(index, block);
    m.push_back(block);
    m.clear();
    { cm.size() } -> std::convertible_to<std::size_t>;
    { cm.height() } -> std::convertible_to<std::size_t>;
    { cm.empty() } -> std::convertible_to<Bool>;
};

/** Árbol de Merkle de cadenas con el hasher por defecto. */
using MerkleStrings = MerkleOf<std::string>;
/** Hasher que usa MerkleStrings. */
using MerkleHasher = StdMerkleHasher<std::string>;

/** Últimos 4 dígitos hexadecimales del hash, para mostrarlo abreviado. */
std::string ShortHash(MerkleStrings::Digest digest) {
    std::ostringstream ss;
    ss << std::hex << std::setw(4) << std::setfill('0') << (digest & 0xffff);
    return ss.str();
}

/** Igual que Check() con el nombre `MerkleTree`. */
void CheckMerkle(Bool ok, const std::string& what) { Check(ok, "MerkleTree", what); }

/** Imprime el árbol con su raíz, cantidad de hojas y altura. */
void ShowMerkle(const MerkleStrings& tree, const std::vector<std::string>& blocks) {
    struct View : decltype(tree.tree_view()) {
        const std::vector<std::string>* blocks;
        void label(std::ostream& os, Handle node) const {
            os << (node->is_leaf() ? (*blocks)[node->getRef()] + ":" : "") << ShortHash(node->getValue());
        }
    };
    visual::print_tree(std::cout, View{{tree.tree_view()}, &blocks});
    std::cout << "raiz = " << ShortHash(tree.root_hash()) << ", hojas = " << tree.size()
              << ", altura = " << tree.height() << "\n\n";
}

/** Primeros 8 dígitos del hash en el formato de Bitcoin. */
std::string ShortHash(const Sha256Digest& digest) { return digest.display_hex().substr(0, 8); }

/** Texto con el que se muestra un bloque de tipo cadena. */
std::string BlockText(const std::string& block) { return block; }
/** Texto con el que se muestra un bloque que ya es un hash. */
std::string BlockText(const Sha256Digest& block) { return ShortHash(block); }

/** Lista de bloques de un árbol de Merkle. */
template <MerkleLike Tree> using Blocks = std::vector<typename Tree::value_type>;

/** Construye un árbol de Merkle con los bloques dados. */
template <MerkleLike Tree> Tree BuildMerkle(const Blocks<Tree>& blocks) { return Tree(blocks.begin(), blocks.end()); }

/** Comprueba que cada prueba verifica y que falla con un bloque o una raíz alterados. */
template <MerkleLike Tree>
void TestMerkleProofs(const Tree& tree, const Blocks<Tree>& blocks, const typename Tree::value_type& extra,
                      const std::string& name) {
    PrintColor("TestMerkleProofs (" + name + ")");
    auto root = tree.root_hash();
    auto otherRoot = Tree{extra}.root_hash();
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        auto proof = tree.proof(i);
        std::cout << "prueba(" << BlockText(blocks[i]) << ") = " << proof.size() << " hashes\n";
        Check(Tree::verify(blocks[i], proof, root), name, "la prueba de " + BlockText(blocks[i]) + " no verifico");
        Check(!Tree::verify(extra, proof, root), name, "bloque alterado verifico");
        Check(!Tree::verify(blocks[i], proof, otherRoot), name, "raiz alterada verifico");
    }
    std::cout << "Todas las pruebas verifican; bloque o raiz alterados fallan\n";
}

/** Comprueba que `update` equivale a reconstruir el árbol, es reversible y no afecta a las copias. */
template <MerkleLike Tree>
void TestMerkleUpdate(Tree& tree, const Blocks<Tree>& blocks, const typename Tree::value_type& extra,
                      const std::string& name) {
    PrintColor("TestMerkleUpdate (" + name + ")");
    auto before = tree.root_hash();
    Tree snapshot(tree);
    for (std::size_t i = 0; i < blocks.size(); ++i) {
        tree.update(i, extra);
        std::cout << "update(" << i << ", " << BlockText(extra) << "): " << ShortHash(before) << " -> "
                  << ShortHash(tree.root_hash()) << '\n';
        Check(tree.root_hash() != before, name, "update no cambio la raiz");
        Check(snapshot.root_hash() == before, name, "la copia no es independiente");
        auto edited = blocks;
        edited[i] = extra;
        Check(BuildMerkle<Tree>(edited).root_hash() == tree.root_hash(), name, "update no coincide con reconstruir");
        tree.update(i, blocks[i]);
        Check(tree.root_hash() == before, name, "restaurar el bloque no restauro la raiz");
    }
    Bool threw = false;
    try { tree.update(blocks.size(), extra); } catch (const std::out_of_range&) { threw = true; }
    Check(threw, name, "update fuera de rango no lanzo");
    std::cout << "update coincide con reconstruir y es reversible\n";
}

/** Prueba el árbol vacío, la copia independiente, `clear` y el árbol de una sola hoja. */
template <MerkleLike Tree> void TestMerkleCopyAndClear(const Blocks<Tree>& blocks, const std::string& name) {
    PrintColor("TestCopyAndClear (" + name + ")");
    Tree tree;
    Bool threw = false;
    try { tree.root_hash(); } catch (const std::logic_error&) { threw = true; }
    Check(tree.empty() && threw, name, "arbol vacio");
    tree = BuildMerkle<Tree>(blocks);
    Tree copy(tree);
    tree.clear();
    Check(tree.empty() && copy.size() == blocks.size(), name, "clear afecto a la copia");
    tree = Tree{blocks[0]};
    Check(tree.height() == 1 && Tree::verify(blocks[0], tree.proof(0), tree.root_hash()), name, "arbol de una hoja");
    std::cout << "clear, copia y arbol de una hoja OK\n";
}

/** Comprueba que añadir bloques uno a uno da la misma raíz y las mismas pruebas que construir de una vez. */
template <MerkleLike Tree>
void TestMerklePushBack(const Blocks<Tree>& blocks, const std::string& name) {
    PrintColor("TestMerklePushBack (" + name + ")");
    Tree tree;
    Blocks<Tree> pushed;
    for (const auto& block : blocks) {
        pushed.push_back(block);
        tree.push_back(block);
        Check(tree.size() == pushed.size(), name, "size tras push_back");
        Check(tree.root_hash() == BuildMerkle<Tree>(pushed).root_hash(), name, "push_back no coincide con reconstruir");
        for (std::size_t i = 0; i < pushed.size(); ++i)
            Check(Tree::verify(pushed[i], tree.proof(i), tree.root_hash()), name, "prueba tras push_back");
    }
    std::cout << "push_back 1.." << blocks.size() << " hojas: raiz y pruebas coinciden con reconstruir\n";
}

/** Comprueba que `<<` y `>>` guardan y recuperan el árbol, y que un texto alterado se rechaza sin cambiar el árbol. */
template <MerkleLike Tree>
void TestMerklePersistence(const Blocks<Tree>& blocks, const std::string& name) {
    PrintColor("TestMerklePersistence (" + name + ")");
    Tree tree = BuildMerkle<Tree>(blocks);
    std::stringstream saved;
    saved << tree;
    Tree loaded;
    saved >> loaded;
    Check(loaded.size() == tree.size() && loaded.root_hash() == tree.root_hash(), name, "el arbol leido no coincide");
    for (std::size_t i = 0; i < blocks.size(); ++i)
        Check(Tree::verify(blocks[i], loaded.proof(i), loaded.root_hash()), name, "prueba del arbol leido");

    std::stringstream none;
    none << Tree{};
    Tree emptied(tree);
    none >> emptied;
    Check(emptied.empty(), name, "leer un arbol vacio no lo vacio");

    std::string text = saved.str();
    Char& digit = text[text.find('(') + 1];
    digit = digit == '9' ? '8' : digit + 1;
    std::istringstream tampered(text);
    Tree target(tree);
    tampered >> target;
    Check(tampered.fail() && target.root_hash() == tree.root_hash(), name, "un texto alterado debio rechazarse");
    std::cout << "escribir y leer " << tree.size() << " hojas OK; un hash alterado se rechaza y el arbol no cambia\n";
}

/** Llama a `update`, `proof` y `root_hash` desde varios hilos y comprueba que el estado final es consistente. */
template <MerkleLike Tree>
void TestMerkleRace(const Blocks<Tree>& blocks, const typename Tree::value_type& extra, const std::string& name) {
    PrintColor("TestRaceCondition (" + name + ")");
    Check(blocks.size() >= std::size_t(NThreads), name, "faltan bloques para la prueba de hilos");
    Tree tree = BuildMerkle<Tree>(blocks);
    std::vector<std::thread> threads;
    for (Int t = 0; t < NThreads; ++t)
        threads.emplace_back([&, t] {
            for (Int i = 0; i < 2000; ++i) {
                tree.update(t, i % 2 ? blocks[t] : extra);
                (void)tree.proof(t);
                (void)tree.root_hash();
            }
        });
    for (auto& thread : threads) thread.join();
    auto edited = blocks;
    for (Int t = 0; t < NThreads; ++t) {
        tree.update(t, extra);
        edited[t] = extra;
    }
    Check(tree.root_hash() == BuildMerkle<Tree>(edited).root_hash(), name,
          "estado inconsistente tras actualizar en paralelo");
    std::cout << NThreads << " hilos con update/proof/root_hash: raiz consistente\n";
}

/** Ejecuta todas las pruebas de Merkle sobre el tipo de árbol indicado. */
template <MerkleLike Tree>
void TestMerkle(const Blocks<Tree>& blocks, const typename Tree::value_type& extra, const std::string& name) {
    Tree tree = BuildMerkle<Tree>(blocks);
    TestMerkleProofs(tree, blocks, extra, name);
    TestMerkleUpdate(tree, blocks, extra, name);
    TestMerklePushBack<Tree>(blocks, name);
    TestMerkleCopyAndClear<Tree>(blocks, name);
    TestMerklePersistence<Tree>(blocks, name);
    TestMerkleRace<Tree>(blocks, extra, name);
}

void DemoMerkleBitcoin() {
    PrintColor("DemoMerkleBitcoin (SHA256 doble, nodo impar duplicado)");
    using Btc = MerkleOf<Sha256Digest, BitcoinMerkleHasher>;
    // bloque 170 de Bitcoin (primera transaccion entre personas)
    Btc block170{Sha256Digest::from_display_hex("b1fea52486ce0c62bb442b530a3f0132b826c74e473d1f2c220bfa78111c5082"),
                 Sha256Digest::from_display_hex("f4184fc596403b9d638783cf57adfe4c75c605f6356fbc91338530e9831e9e16")};
    std::cout << "raiz = " << block170.root_hash().display_hex() << '\n';
    CheckMerkle(block170.root_hash().display_hex() == "7dac2c5666815c17a3b36427de37bb9d2e2c5ccec3f8633eb91a4205cb4c10ff",
                "la raiz del bloque 170 no coincide con Bitcoin");

    Sha256Digest a = Sha256::hash("a"), b = Sha256::hash("b"), c = Sha256::hash("c");
    CheckMerkle(Btc{a, b, c}.root_hash() == Btc{a, b, c, c}.root_hash(), "[a,b,c] debe dar la misma raiz que [a,b,c,c]");
    CheckMerkle(Btc{a}.root_hash() == a, "una sola hoja: la raiz es el txid");
    std::cout << "raiz del bloque 170, duplicado del impar y hoja unica OK\n";

    Blocks<Btc> txids;
    for (Char letter = 'A'; letter <= 'I'; ++letter) txids.push_back(Sha256::hash(std::string(1, letter)));
    TestMerkle<Btc>(txids, Sha256::hash("Z"), "Merkle Bitcoin");
}

void DemoMerkleTree() {
    PrintColor("DemoMerkleTree");
    const std::vector<std::string> blocks{"A", "B", "C", "D", "E"};
    MerkleStrings tree(blocks.begin(), blocks.end());

    PrintColor(tree);
    TestTraversal(tree, "MerkleTree");
    tree.print_tree(std::cout, visual::Layout::Sideways);
    tree.print_tree(std::cout, visual::Layout::Cursor);
    TestTreeTraversals(tree, "MerkleTree");

    TestMerkle<MerkleStrings>({"A", "B", "C", "D", "E", "F", "G", "H", "I"}, "Z", "MerkleTree");
}

void DemoMerkleTreeStepByStep() {
    PrintColor("DemoMerkleTree paso a paso");
    std::vector<std::string> blocks{"A", "B", "C", "D", "E"};
    MerkleStrings tree(blocks.begin(), blocks.end());

    PrintColor("build(A, B, C, D, E): hojas H(bloque), padres H(izq || der), la hoja impar sube", "1;33");
    ShowMerkle(tree, blocks);

    auto before = tree.root_hash();
    blocks[2] = "C2";
    tree.update(2, blocks[2]);
    PrintColor("update(2, \"C2\"): solo cambian los hashes de su camino hasta la raiz", "1;33");
    ShowMerkle(tree, blocks);
    CheckMerkle(before != tree.root_hash(), "update no cambio la raiz");

    const Ref target = 3;
    PrintColor("proof(" + std::to_string(target) + "): hermanos necesarios para llegar a la raiz", "1;33");
    auto hash = MerkleHasher::leaf(blocks[target]);
    std::cout << "h0 = H(" << blocks[target] << ") = " << ShortHash(hash) << '\n';
    Int level = 0;
    for (auto&& step : tree.proof(target)) {
        const auto& s = step.getValue();
        hash = s.sibling_is_right ? MerkleHasher::combine(hash, s.sibling) : MerkleHasher::combine(s.sibling, hash);
        std::string prev = "h" + std::to_string(level++);
        std::cout << "h" << level << " = H("
                  << (s.sibling_is_right ? prev + " || " + ShortHash(s.sibling) : ShortHash(s.sibling) + " || " + prev)
                  << ") = " << ShortHash(hash) << '\n';
    }
    Bool ok = hash == tree.root_hash();
    PrintColor("h" + std::to_string(level) + " == raiz (" + ShortHash(tree.root_hash()) + "): " +
                   (ok ? "verifica" : "NO verifica"),
               ok ? "1;32" : "1;31");
    CheckMerkle(ok, "la prueba paso a paso no verifico");
    CheckMerkle(!MerkleStrings::verify("D2", tree.proof(target), tree.root_hash()), "bloque alterado verifico");
    PrintColor("verify con el bloque alterado \"D2\": no verifica", "1;32");
}
