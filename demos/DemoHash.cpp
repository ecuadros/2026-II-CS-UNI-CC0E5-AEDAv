/** @file
 * Demos de la tabla hash: interfaz de mapa y versión paso a paso.
 */

#include "utils/avl_steps.h"

/** Contenedor con interfaz de mapa al estilo de `std::map`. */
template <typename M>
concept MapLike = requires(M& m, const M& cm, const typename M::key_type& key, const typename M::value_type& entry,
                           typename M::iterator it) {
    { m[key] } -> std::same_as<typename M::mapped_type&>;
    { m.at(key) } -> std::same_as<typename M::mapped_type&>;
    { cm.at(key) } -> std::same_as<const typename M::mapped_type&>;
    { m.insert(entry).second } -> std::convertible_to<Bool>;
    { m.emplace(entry).second } -> std::convertible_to<Bool>;
    { m.find(key) } -> std::same_as<typename M::iterator>;
    { cm.find(key) } -> std::same_as<typename M::const_iterator>;
    { m.erase(key) } -> std::convertible_to<std::size_t>;
    { m.erase(it) } -> std::same_as<typename M::iterator>;
    { cm.contains(key) } -> std::convertible_to<Bool>;
    { cm.size() } -> std::convertible_to<std::size_t>;
    { it == m.end() } -> std::convertible_to<Bool>;
    { ++it } -> std::same_as<typename M::iterator&>;
    { it++ } -> std::same_as<typename M::iterator>;
    { it->first } -> std::same_as<const typename M::key_type&>;
    { it->second } -> std::same_as<typename M::mapped_type&>;
    { (*it).second } -> std::same_as<typename M::mapped_type&>;
    { cm.begin()->second } -> std::same_as<const typename M::mapped_type&>;
    { cm.rbegin() == cm.rend() } -> std::convertible_to<Bool>;
    requires std::convertible_to<typename M::iterator, typename M::const_iterator>;
};

/** Prueba la interfaz de mapa: acceso, búsqueda, inserción, borrado, recorrido, lectura y escritura. */
template <MapLike Map> void TestMap(const std::string& name) {
    auto check = [&](Bool ok, const std::string& what) { Check(ok, name, what); };
    Map edades;
    edades["ana"] = 21;
    edades["luis"] = 25;
    edades["eva"] = 31;
    check(edades["nadie"] == 0 && edades.size() == 4, "operator[] no creo la clave nueva");

    if (auto it = edades.find("luis"); it != edades.end()) it->second += 5;
    check(edades["luis"] == 30, "find no modifico el valor");
    check(edades.erase("nadie") == 1 && edades.erase("nadie") == 0, "erase(clave) fallo");
    check(edades.find("pepe") == edades.end() && !edades.contains("pepe") && edades.contains("ana"),
          "find/contains fallo");

    auto [nuevo, insertado] = edades.insert({"rosa", 40});
    check(insertado && nuevo->first == "rosa" && nuevo->second == 40, "insert fallo");
    check(!edades.insert({"rosa", 99}).second && !edades.emplace("rosa", 99).second && edades["rosa"] == 40,
          "insert/emplace sobrescribio una clave existente");
    check(edades.emplace("beto", 18).second && edades.size() == 5, "emplace fallo");

    std::string claves;
    for (auto&& [clave, edad] : edades) {
        claves += clave + " ";
        ++edad;
    }
    check(claves == "ana beto eva luis rosa " && edades["rosa"] == 41, "recorrido [clave, valor] fallo");
    const auto& lectura = edades;
    Int suma = 0;
    for (const auto& [clave, edad] : lectura) suma += edad;
    for (auto it = lectura.rbegin(); it != lectura.rend(); ++it) suma -= it->second;
    check(suma == 0 && lectura.find("ana")->second == 22 && (*edades.begin()).first == "ana", "recorrido const fallo");

    typename Map::const_iterator fijo = edades.find("beto");
    check(fijo++->first == "beto" && fijo->first == "eva" && lectura.at("eva") == 32, "it++/const_iterator/at fallo");
    edades.at("eva") = 33;
    Bool lanzo = false;
    try {
        edades.at("pepe");
    } catch (const std::out_of_range&) {
        lanzo = true;
    }
    check(lanzo && edades["eva"] == 33 && edades.size() == 5, "at fallo");
    auto siguiente = edades.erase(edades.find("eva"));
    check(siguiente->first == "luis" && edades.erase("rosa") == 1 && edades.size() == 3, "erase(it) fallo");
    check(edades.erase(edades.find("luis")) == edades.end(), "erase del ultimo no devolvio end()");
    check(edades.remove("beto") && !edades.remove("beto") && edades.size() == 1, "remove fallo");

    std::stringstream stream("[(zoe,40),(beto,18),(ana,22)]");
    stream >> edades;
    check(stream && edades.size() == 3 && edades["zoe"] == 40, "operator>> fallo");
    stream.clear();
    stream.str("");
    stream << edades;
    check(stream.str() == "[(ana,22),(beto,18),(zoe,40)]", "operator<< fallo");

    const std::string rara = R"( di "hola", \ (x) [y])";
    edades[rara] = 7;
    stream.clear();
    stream.str("");
    stream << edades;
    std::cout << name << " con clave entre comillas: " << stream.str() << '\n';
    Map leida;
    stream >> leida;
    check(stream && leida.size() == 4 && leida[rara] == 7, "clave con comillas y delimitadores no se recupero");
    std::istringstream sin_cerrar(R"([(ana,1),("abierta,2)])");
    sin_cerrar >> leida;
    check(sin_cerrar.fail() && leida.size() == 4, "comilla sin cerrar debio rechazarse");
    edades.erase(rara);
    TestInvalidRead(edades, name);
    std::cout << name << " leido de texto desordenado: " << edades << '\n';
    edades.print_tree(std::cout);
    std::cout << name << ": interfaz de mapa OK\n";
}

void DemoHashTable() {
    PrintColor("DemoHashTable (interfaz de mapa, clave string)");
    TestMap<HashTableOf<std::string, Int>>("HashTable");
    TestTree<HashTableOf<TX, Ref>::Tree>("HashTable (AVL interno)");
}

void DemoHashTableStepByStep() {
    PrintColor("DemoHashTable paso a paso (AVL ordenado por clave)");
    using Table = HashTableOf<std::string, Int>;
    Table table;
    Int age = 18;
    for (std::string key : {"ana", "luis", "eva", "juan", "rosa", "pedro", "maria", "jose"}) {
        std::string title = "tabla[\"" + key + "\"] = " + std::to_string(age);
        AVLStep(table, title, [&](auto& t, auto& s) {
            s.try_insert(key, 0);
            t[key] = age;
        });
        ++age;
    }
    for (std::string key : {"eva", "rosa", "ana"})
        AVLStep(table, "remove(\"" + key + "\")", [&](auto& t, auto& s) {
            s.remove(key);
            t.remove(key);
        });
    std::cout << "Tabla final: " << table << '\n';
}
