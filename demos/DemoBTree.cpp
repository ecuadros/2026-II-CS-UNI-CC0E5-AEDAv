/** @file
 * Demo del árbol B*: pruebas genéricas y pruebas propias (caracteres, rangos, persistencia, concurrencia).
 */

#include "utils/generic_tests.h"

/** Pares (valor, ref) del árbol en orden. */
template <typename Tree> std::vector<std::pair<typename Tree::value_type, Ref>> Dump(const Tree& tree) {
    std::vector<std::pair<typename Tree::value_type, Ref>> out;
    for (auto&& node : tree.InOrder())
        out.emplace_back(node.getValue(), node.getRef());
    return out;
}

/** Guarda @p tree en un archivo, lo lee en un árbol nuevo y exige que tenga exactamente los mismos pares. */
template <typename Tree> Tree RoundTrip(const Tree& tree, const std::string& label, const std::string& file) {
    Tree loaded;
    WithDemoFile(file, [&] {
        SaveTo(file, tree);
        LoadFrom(file, loaded);
    });
    auto sorted = [](auto d) { std::sort(d.begin(), d.end()); return d; };
    if (sorted(Dump(tree)) != sorted(Dump(loaded)))
        throw std::runtime_error("BTree persistencia: " + label + " no coincide");
    auto d = Dump(loaded);
    typename Tree::Compare comp;
    if (!std::is_sorted(d.begin(), d.end(), [&](const auto& x, const auto& y) { return comp(x.first, y.first); }))
        throw std::runtime_error("BTree persistencia: " + label + " perdio el orden");
    std::cout << "  " << label << ": " << loaded.size() << " elementos OK\n";
    return loaded;
}

/** Prueba lower_bound, upper_bound y range contra un recorrido completo, con y sin duplicados. */
static_assert(NothrowMovable<BTreeOf<TX>>, "BTree: el move debe ser noexcept");
static_assert(OptionalLookup<BTreeOf<TX>> && RangeQueryable<BTreeOf<TX>>, "BTree: faltan find/range");
static_assert(BTreePageConcept<BTreePage<BTreeTraits<TX>>>, "BTree: la pagina no cumple BTreePageConcept");

template <RangeQueryable Tree> void TestRange() {
    PrintColor("TestBTreeRange");
    std::mt19937 rng(777);
    std::vector<TX> keys;
    for (TX i = 0; i < 2000; ++i)
        keys.push_back(i / 2 * 3);
    std::shuffle(keys.begin(), keys.end(), rng);
    Tree tree;
    for (TX k : keys)
        tree.insert(k, k);
    std::sort(keys.begin(), keys.end());

    Tree empty;
    if (empty.lower_bound(0) != std::as_const(empty).end() || empty.upper_bound(0) != std::as_const(empty).end())
        throw std::runtime_error("BTree range: el arbol vacio debe dar end()");

    for (TX probe = -2; probe <= 3003; ++probe) {
        auto lo = std::lower_bound(keys.begin(), keys.end(), probe);
        auto hi = std::upper_bound(keys.begin(), keys.end(), probe);
        auto tl = tree.lower_bound(probe);
        auto tu = tree.upper_bound(probe);
        if ((lo == keys.end()) != (tl == std::as_const(tree).end()) || (hi == keys.end()) != (tu == std::as_const(tree).end()) ||
            (lo != keys.end() && (*tl).getValue() != *lo) || (hi != keys.end() && (*tu).getValue() != *hi))
            throw std::runtime_error("BTree range: lower/upper_bound incorrecto en " + std::to_string(probe));
    }
    for (auto [low, high] : std::vector<std::pair<TX, TX>>{{0, 0}, {-10, 5}, {10, 9}, {100, 400}, {2990, 5000}, {-5, 5000}, {4, 5}}) {
        std::vector<TX> got, expected;
        tree.range(low, high, [&](const auto& node) { got.push_back(node.getValue()); });
        for (TX k : keys)
            if (k >= low && k <= high) expected.push_back(k);
        if (got != expected)
            throw std::runtime_error("BTree range: range(" + std::to_string(low) + "," + std::to_string(high) + ") incorrecto");
    }
    std::cout << "  lower_bound, upper_bound y range coinciden con std::lower_bound/upper_bound OK\n";
}

/** Prueba la persistencia con casos límite: vacío, un elemento, duplicados, negativos, órdenes, comparador, texto inválido. */
void TestBTreePersistence() {
    PrintColor("TestBTreePersistence");
    const std::string file = "btree_persist_test.txt";

    BTreeOf<TX> empty;
    RoundTrip(empty, "vacio", file);

    BTreeOf<TX> one;
    one.insert(42, 7);
    RoundTrip(one, "un elemento", file);

    BTreeOf<TX> dup;
    for (Ref i = 0; i < 20; ++i)
        dup.insert(static_cast<TX>(i % 4), i);
    RoundTrip(dup, "duplicados", file);

    BTreeOf<TX> extremes;
    for (TX k : {std::numeric_limits<TX>::min(), -1, 0, 1, std::numeric_limits<TX>::max()})
        extremes.insert(k, static_cast<Ref>(k));
    RoundTrip(extremes, "negativos y extremos", file);

    std::mt19937 rng(12345);
    std::vector<TX> keys(5000);
    std::iota(keys.begin(), keys.end(), -2500);
    std::shuffle(keys.begin(), keys.end(), rng);
    BTreeOf<TX, std::less<TX>, 3> o3;
    BTreeOf<TX, std::less<TX>, 4> o4;
    BTreeOf<TX, std::less<TX>, 7> o7;
    BTreeOf<TX, std::greater<TX>, 5> desc;
    for (TX k : keys) {
        o3.insert(k, k * 2);
        o4.insert(k, k * 2);
        o7.insert(k, k * 2);
        desc.insert(k, k * 2);
    }
    RoundTrip(o3, "5000 aleatorios orden 3", file);
    RoundTrip(o4, "5000 aleatorios orden 4", file);
    RoundTrip(o7, "5000 aleatorios orden 7", file);
    RoundTrip(desc, "5000 aleatorios descendente", file);

    for (std::size_t i = 0; i < keys.size(); i += 2)
        o3.remove(keys[i]);
    auto afterRemove = RoundTrip(o3, "tras eliminar la mitad", file);
    afterRemove.insert(100000, 1);
    if (o3.contains(100000)) throw std::runtime_error("BTree persistencia: el arbol leido comparte estado");

    auto viaMove = RoundTrip(o4, "copia por move", file);
    BTreeOf<TX, std::less<TX>, 4> moved(std::move(o4));
    if (Dump(moved) != Dump(viaMove)) throw std::runtime_error("BTree persistencia: move cambio el contenido");

    std::stringstream twice;
    twice << o7;
    BTreeOf<TX, std::less<TX>, 7> a;
    twice >> a;
    std::stringstream again;
    again << a;
    if (twice.str() != again.str()) throw std::runtime_error("BTree persistencia: guardar-leer-guardar difiere");
    std::cout << "  guardar-leer-guardar idempotente OK\n";

    BTreeOf<TX> keep;
    keep.insert(1, 10);
    keep.insert(2, 20);
    for (const char* bad : {"", "[", "[(1,2)", "[(1,2),]", "(1,2)", "[(a,b)]", "[(1,2)(3,4)]", "[(1,2);(3,4)]", "basura"})
        TestInvalidRead(keep, std::string("BTree persistencia (") + bad + ")", bad);
    std::cout << "  texto invalido: falla y no modifica el arbol OK\n";

    BTreeOf<std::string> words;
    for (const char* w : {"manzana", "pera", "uva", "kiwi", "a b", "x,y", "(p)", "[q]"})
        words.insert(w, static_cast<Ref>(std::strlen(w)));
    RoundTrip(words, "cadenas con espacios, comas y parentesis", file);
}

/** Prueba la concurrencia del BTree: try_insert disputado y escrituras mezcladas con lecturas (las inserciones disjuntas ya las cubre DemoRaceCondition dentro de Demo). */
void TestBTreeRace() {
    PrintColor("TestBTreeRace");

    BTreeOf<TX> unique;
    std::atomic<int> winners{0};
    RunThreads(NThreads, [&](Int) {
        for (TX i = 0; i < 20000; ++i)
            if (unique.try_insert(i, i)) ++winners;
    });
    if (winners != 20000 || unique.size() != 20000)
        throw std::runtime_error("BTree race: try_insert acepto duplicados o perdio claves");
    CheckTree(unique, "BTree race try_insert");
    std::cout << "  try_insert disputado: exactamente un ganador por clave OK\n";

    const TX M = 5000;
    BTreeOf<TX> mixed;
    for (TX i = 0; i < M; ++i)
        mixed.insert(i, i);
    std::atomic<Bool> stop{false};
    std::atomic<std::size_t> reads{0};
    std::vector<std::thread> readers;
    for (int r = 0; r < 2; ++r)
        readers.emplace_back([&] {
            while (!stop) {
                std::size_t count = 0;
                TX previous = std::numeric_limits<TX>::min();
                mixed.call([&](const auto& node) {
                    if (node.getValue() < previous) std::terminate();
                    previous = node.getValue();
                    ++count;
                });
                (void)mixed.contains(static_cast<TX>(count % M));
                ++reads;
            }
        });
    RunThreads(NThreads, [&](Int t) {
        for (TX i = t; i < M; i += NThreads)
            if (i % 2 == 0) mixed.remove(i);
            else mixed.insert(M + i, i);
    });
    stop = true;
    for (auto& th : readers)
        th.join();
    CheckTree(mixed, "BTree race mixto");
    for (TX i = 0; i < M; ++i)
        if (mixed.contains(i) != (i % 2 == 1)) throw std::runtime_error("BTree race: estado final incorrecto");
    if (mixed.size() != static_cast<size_t>(M)) throw std::runtime_error("BTree race: tamano final incorrecto");
    std::cout << "  insert/remove concurrentes con " << reads << " recorridos de lectura OK\n";
}

void DemoBTree() {
    Demo<BTreeOf<TX>, BTreeOf<TX, std::greater<TX>>>("BTree");

    const std::string inserts = "D1XJ2xTg8zKL9AhijOPQcEowRSp0NbW567BUfCqrs4FdtYZakHIuvGV3eMylmn";
    const std::string searches = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    const std::string removals = "DYZakHIUwxVJ203ejOP9Qc8AdtuEop1XvTRghSNbW567BfiCqrs4FGMyzKLlmn";

    PrintColor("DemoBTree (caracteres, orden 3)");
    BTreeOf<Char> tree;
    for (std::size_t i = 0; i < inserts.size(); ++i) {
        tree.try_insert(inserts[i], static_cast<Ref>(i * i));
        CheckTree(tree, std::string("BTree insert ") + inserts[i]);
    }
    if (tree.try_insert(inserts[0], 0)) throw std::runtime_error("BTree: try_insert acepto un duplicado");
    std::cout << "size = " << tree.size() << ", height = " << tree.height() << '\n';
    tree.print_tree(std::cout);
    tree.print_tree(std::cout, visual::Layout::Sideways);

    for (Char key : searches) {
        auto ref = tree.find_ref(key);
        if (!ref) throw std::runtime_error(std::string("BTree: no se encontro ") + key);
        std::cout << key << "->" << *ref << ' ';
    }
    std::cout << '\n';

    RoundTrip(tree, "BTree caracteres (guardar y leer)", "btree.txt");

    for (std::size_t i = 0; i < removals.size(); ++i) {
        if (!tree.remove(removals[i]) || tree.contains(removals[i]))
            throw std::runtime_error(std::string("BTree: no se elimino ") + removals[i]);
        CheckTree(tree, std::string("BTree remove ") + removals[i]);
        if (i == removals.size() / 2) {
            std::cout << "A mitad de las eliminaciones (height = " << tree.height() << "):\n";
            tree.print_tree(std::cout);
        }
    }
    if (!tree.empty() || tree.remove('D')) throw std::runtime_error("BTree: el arbol deberia quedar vacio");
    std::cout << "BTree: inserciones, busquedas y eliminaciones OK\n";

    TestRange<BTreeOf<TX, std::less<TX>, 4>>();
    TestBTreePersistence();
    TestBTreeRace();
}
