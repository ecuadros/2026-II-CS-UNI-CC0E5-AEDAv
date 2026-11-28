/**
 * @file btree_layout_test.cpp
 * @ingroup layouts
 * @brief Pruebas de `BTreeLayout` (topología, centrado, solapamiento, resaltado, modos `Bus`/`Edges`),
 *        del rasterizador de `ConsoleGraphRenderer` y de la composición lado a lado.
 *
 * Retorna 0 si todo pasa; 1 en caso contrario.
 */

#include "../containers/trees/btree.h"
#include "../containers/trees/bstar_tree.h"
#include "../utilities/layouts.h"
#include "../utilities/side_by_side.h"
#include "../types.h"
#include "../containers/trees/btree_validator.h"
#include <algorithm>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

Size g_checks   = 0;   /**< Verificaciones ejecutadas. */
Size g_failures = 0;   /**< Verificaciones fallidas. */

/** @brief Registra una verificación e informa si falla. */
void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failures;
        std::cout << "  [FALLA] " << what << '\n';
    }
}

const std::vector<TX> Keys{50, 20, 80, 10, 30, 70, 90, 5, 15, 25, 35, 60, 75, 85, 95};   /**< ~15 claves de prueba. */

/** @brief Nodos de un `VisualGraph` como vector. */
std::vector<VisualNode> nodesOf(const VisualGraph& graph) {
    std::vector<VisualNode> out;
    for (const auto& item : graph.nodes) out.push_back(item.getValue());
    return out;
}

/** @brief Aristas de un `VisualGraph` como vector. */
std::vector<VisualEdge> edgesOf(const VisualGraph& graph) {
    std::vector<VisualEdge> out;
    for (const auto& item : graph.edges) out.push_back(item.getValue());
    return out;
}

/** @brief Ancho visible como coordenada. */
TX widthOf(const std::string& text) {
    TX width = visible_length(text);
    return width;
}

/** @brief Convierte una cantidad en coordenada o id. */
TX toCoord(Size n) {
    TX value = n;
    return value;
}

/** @brief Quita las secuencias SGR `\033[...m`. */
std::string stripAnsi(const std::string& text) {
    std::string out;
    for (Index i{}; i < text.size(); ++i) {
        if (text[i] == '\033') {
            while (i < text.size() && text[i] != 'm') ++i;
            continue;
        }
        out += text[i];
    }
    return out;
}

/** @brief Subcadena por columnas visibles de un texto plano UTF-8. */
std::string columns(const std::string& line, Size from, Size count) {
    std::string out;
    Size col{};
    for (Index i{}; i < line.size(); ) {
        Size len = 1;
        if      ((line[i] & 0xE0) == 0xC0) len = 2;
        else if ((line[i] & 0xF0) == 0xE0) len = 3;
        else if ((line[i] & 0xF8) == 0xF0) len = 4;
        if (col >= from && col < from + count) out += line.substr(i, len);
        ++col;
        i += len;
    }
    return out;
}

/** @brief Páginas del árbol en orden por niveles (mismo orden que los ids del layout) con su profundidad. */
template <typename Tree>
std::vector<std::pair<const typename Tree::Page*, TX>> pagesByLevel(const Tree& tree) {
    std::vector<std::pair<const typename Tree::Page*, TX>> pages;
    if (tree.getRoot()) pages.push_back({tree.getRoot(), 0});
    for (Index i{}; i < pages.size(); ++i)
        for (Index c{}; c < pages[i].first->childCount(); ++c)
            pages.push_back({pages[i].first->child(c), pages[i].second + 1});
    return pages;
}

/**
 * @brief Verifica topología, etiquetas, centrado y ausencia de solapamiento de un árbol dibujado.
 * @param tree Árbol (BTree o BStarTree).
 * @param tag Prefijo de los mensajes.
 */
template <typename Tree>
void checkDrawing(const Tree& tree, const std::string& tag) {
    const auto pages = pagesByLevel(tree);
    const TX   count = toCoord(pages.size());   // ids de página: 0 .. count-1

    // --- Modo Edges: topología pura
    VisualGraph edgesGraph = BTreeLayout{PlainBTreeFormatter{}}.with_links(BTreeLinks::Edges)(tree);
    auto nodes = nodesOf(edgesGraph);
    auto edges = edgesOf(edgesGraph);
    check(nodes.size() == pages.size(), tag + "Edges: nodos visuales = paginas");
    check(edges.size() + 1 == pages.size(), tag + "Edges: aristas = paginas - 1");

    bool labels = true, levels = true, noRef = true;
    for (const VisualNode& n : nodes) {
        const auto& [page, depth] = pages[n.id];
        labels = labels && n.label == btree_page_label(*page);
        levels = levels && n.y == depth * 3;
        noRef  = noRef && n.label.find('(') == std::string::npos;
    }
    check(labels, tag + "etiqueta [ a │ b ] de cada pagina en orden por niveles");
    check(noRef, tag + "las etiquetas no incluyen Ref");
    check(levels, tag + "y = profundidad x 3");

    bool edgesOk = true;
    for (const VisualEdge& e : edges) {
        const auto* parent = pages[e.from_id].first;
        const auto* child  = pages[e.to_id].first;
        edgesOk = edgesOk && child->parent() == parent;
    }
    check(edgesOk, tag + "cada arista une una pagina con su hijo");

    // Centrado: centro del padre = (centro del primer hijo + centro del último) / 2
    auto center = [](const VisualNode& n) { return n.x + widthOf(n.label) / 2; };
    std::map<TX, std::vector<VisualNode>> children;
    for (const VisualEdge& e : edges) children[e.from_id].push_back(nodes[e.to_id]);
    bool centered = true;
    for (auto& [id, kids] : children) {
        std::sort(kids.begin(), kids.end(), [](const VisualNode& a, const VisualNode& b) { return a.x < b.x; });
        centered = centered && center(nodes[id]) == (center(kids.front()) + center(kids.back())) / 2;
    }
    check(centered, tag + "cada pagina interna centrada entre su primer y su ultimo hijo");

    // Sin solapamiento horizontal en un mismo nivel
    std::map<TX, std::vector<VisualNode>> rows;
    for (const VisualNode& n : nodes) rows[n.y].push_back(n);
    bool separated = true;
    for (auto& [y, row] : rows) {
        std::sort(row.begin(), row.end(), [](const VisualNode& a, const VisualNode& b) { return a.x < b.x; });
        for (Index i = 1; i < row.size(); ++i)
            separated = separated && row[i].x >= row[i - 1].x + widthOf(row[i - 1].label) + 1;
    }
    check(separated, tag + "ningun solapamiento entre nodos del mismo nivel");

    // --- Modo Bus: mismas posiciones de página + conectores sobre cada hijo
    VisualGraph busGraph = BTreeLayout{PlainBTreeFormatter{}}(tree);
    auto busNodes = nodesOf(busGraph);
    check(busGraph.edges.size() == Size{}, tag + "Bus: sin aristas (los conectores son nodos)");
    bool samePlace = busNodes.size() >= nodes.size();
    for (Index i{}; samePlace && i < nodes.size(); ++i)
        samePlace = busNodes[i].id == nodes[i].id && busNodes[i].x == nodes[i].x && busNodes[i].y == nodes[i].y;
    check(samePlace, tag + "Bus: paginas en las mismas posiciones que en Edges");

    auto lines = console_lines(busGraph);
    bool drawn = true, linked = true;
    for (const VisualNode& n : busNodes) {
        if (n.id >= count) continue;   // conector
        drawn = drawn && n.y < toCoord(lines.size())
                      && columns(lines[n.y], n.x, visible_length(n.label)) == n.label;
        if (n.y > 0) linked = linked && columns(lines[n.y - 1], center(n), 1) == "│";
    }
    check(drawn, tag + "rasterizado: cada etiqueta aparece en su (x, y)");
    check(linked, tag + "Bus: barra vertical sobre cada pagina no raiz");
}

/** @brief Casos borde y árboles de ~15 claves. */
template <typename Tree>
void runLayout(const std::string& name, Size order) {
    const std::string tag = name + ": ";

    Tree empty(order);
    VisualGraph g = BTreeLayout{}(empty);
    check(g.nodes.size() == Size{} && g.edges.size() == Size{} && g.width == 0, tag + "arbol vacio -> grafo vacio");
    check(console_lines(g).empty(), tag + "arbol vacio -> sin lineas");

    Tree single(order);
    single.insert(7, 70);
    VisualGraph one = BTreeLayout{PlainBTreeFormatter{}}.with_links(BTreeLinks::Edges)(single);
    check(one.nodes.size() == Size{1} && one.edges.size() == Size{} && nodesOf(one)[0].label == "[ 7 ]",
          tag + "un nodo: [ 7 ] sin aristas");
    check(BTreeLayout{PlainBTreeFormatter{}}(single).nodes.size() == Size{1}, tag + "un nodo en modo Bus sin conectores");

    Tree tree(order);
    for (TX k : Keys) tree.insert(k, k * 10);
    check(validateBTree(tree).empty() && tree.height() >= Size{2}, tag + "arbol de 15 claves valido con 2+ niveles");
    checkDrawing(tree, tag);

    // Resaltado con formatter propio (lambda que recibe página y rol)
    auto marker = [](const typename Tree::Page& page, BTreeRole role) {
        return (role == BTreeRole::Highlight ? "<H>" : "") + btree_page_label(page);
    };
    auto path = btree_search_path(tree, 25);
    auto marked = nodesOf(BTreeLayout{highlight_pages(path, marker)}(tree));
    Size highlighted = std::count_if(marked.begin(), marked.end(), [](const VisualNode& n) { return n.label.rfind("<H>", 0) == 0; });
    check(path.size() == tree.height() && highlighted == path.size(), tag + "camino de busqueda resaltado (una pagina por nivel)");
    auto owner = nodesOf(BTreeLayout{highlight_pages(btree_page_of(tree, 25), marker)}(tree));
    check(std::count_if(owner.begin(), owner.end(), [](const VisualNode& n) { return n.label.rfind("<H>", 0) == 0; }) == 1,
          tag + "btree_page_of resalta solo la pagina que contiene la clave");
    check(btree_page_of(tree, 999).size() == Size{}, tag + "btree_page_of de clave ausente es vacio");

    // Formatter por defecto: roles con color y resaltado con fondo rojo
    auto colored = nodesOf(BTreeLayout{highlight_pages(btree_page_of(tree, 25))}(tree));
    bool red = false, magenta = false;
    for (const VisualNode& n : colored) {
        red     = red     || n.label.find("\033[41;37;1m") != std::string::npos;
        magenta = magenta || n.label.find("\033[35;1;4m")  != std::string::npos;
    }
    check(red && magenta, tag + "formatter por defecto: raiz magenta y resaltado con fondo rojo");

    // Drawable: tree.draw(layout, renderer)
    std::ostringstream os;
    tree.draw(BTreeLayout{}, ConsoleGraphRenderer(os));
    auto drawnLines = rasterize_console(os.str());
    check(!drawnLines.empty() && stripAnsi(drawnLines[0]).find(btree_page_label(*tree.getRoot())) != std::string::npos,
          tag + "Drawable dibuja la raiz en la primera linea");
}

/** @brief Composición lado a lado con y sin ANSI, y apilado cuando no cabe. */
template <typename Tree>
void runSideBySide(const std::string& name) {
    const std::string tag = name + " lado a lado: ";
    Tree tree(4);
    for (TX k = 1; k <= 7; ++k) tree.insert(k, k);
    VisualGraph plainBefore = BTreeLayout{PlainBTreeFormatter{}}(tree);
    VisualGraph colorBefore = BTreeLayout{highlight_pages(btree_search_path(tree, 8))}(tree);
    tree.insert(8, 8);
    VisualGraph plainAfter = BTreeLayout{PlainBTreeFormatter{}}(tree);
    VisualGraph colorAfter = BTreeLayout{highlight_pages(btree_page_of(tree, 8))}(tree);

    auto aligned = [](const std::string& text) {
        std::istringstream is(text);
        std::string line;
        std::vector<Size> cols;
        while (std::getline(is, line)) {
            auto at = line.find("┃");
            if (at == std::string::npos) at = line.find("╋");
            if (at == std::string::npos) return false;
            cols.push_back(visible_length(line.substr(0, at)));
        }
        return !cols.empty() && std::all_of(cols.begin(), cols.end(), [&](Size c) { return c == cols.front(); });
    };
    std::string plain = side_by_side(plainBefore, "Antes", plainAfter, "Despues");
    std::string color = side_by_side(colorBefore, "Antes", colorAfter, "Despues");
    check(aligned(plain), tag + "sin ANSI: separador en la misma columna en todas las lineas");
    check(aligned(color), tag + "con ANSI: separador alineado (relleno por visible_length)");
    check(stripAnsi(color) == plain, tag + "con y sin ANSI producen el mismo texto visible");
    check(plain.find("Antes") != std::string::npos && plain.find("Despues") != std::string::npos, tag + "titulos presentes");

    std::string stacked = side_by_side(plainBefore, "Antes", plainAfter, "Despues", 20);
    Size lines = std::count(stacked.begin(), stacked.end(), '\n');
    Size expected = 2 + console_lines(plainBefore).size() + 1 + 2 + console_lines(plainAfter).size();
    check(stacked.find("┃") == std::string::npos && lines == expected, tag + "si supera el maximo se apilan uno debajo del otro");

    VisualGraph none = BTreeLayout{}(Tree(4));
    check(aligned(side_by_side(none, "Vacio", plainAfter, "Despues")), tag + "un lado vacio sigue alineado");
}

} // namespace

TX main() {
    runLayout<BTree<BTreeAscTraits<TX>>>("BTree Asc orden 3", 3);
    runLayout<BTree<BTreeDescTraits<TX>>>("BTree Desc orden 3", 3);
    runLayout<BStarTree<BTreeAscTraits<TX>>>("BStarTree Asc orden 4", 4);
    runLayout<BStarTree<BTreeDescTraits<TX>>>("BStarTree Desc orden 4", 4);
    runSideBySide<BTree<BTreeAscTraits<TX>>>("BTree");
    runSideBySide<BStarTree<BTreeAscTraits<TX>>>("BStarTree");
    std::cout << "Verificaciones: " << g_checks << ", fallas: " << g_failures << '\n';
    if (g_failures == 0) return 0;
    return 1;
}
