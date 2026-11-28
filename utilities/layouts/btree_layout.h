/**
 * @file btree_layout.h
 * @ingroup layouts
 * @brief Layout n-ario para árboles B y B*, con etiquetado y resaltado por página.
 */

#ifndef __LAYOUTS_BTREE_LAYOUT_H__
#define __LAYOUTS_BTREE_LAYOUT_H__

#include <algorithm>
#include <initializer_list>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "base_layout.h"
#include "../ansi_style.h"

/** @brief Roles semánticos de una página del árbol B al proyectarla. */
enum class BTreeRole { Root, Internal, Leaf, Highlight };

/**
 * @brief Conexiones entre páginas: bus de consola o aristas del grafo.
 *
 * `Bus` añade conectores para que las etiquetas de los hermanos no oculten
 * el tramo horizontal del renderizador. `Edges` genera un nodo por página
 * y una arista por enlace padre-hijo.
 */
enum class BTreeLinks { Bus, Edges };

/**
 * @brief Etiqueta de una página con SOLO los valores de sus claves: `[ 20 │ 40 ]`.
 * @tparam Page Tipo de página (`BTreePage<T, Compare>`).
 * @param page Página a etiquetar.
 * @return Texto plano `[ k0 │ k1 │ ... ]`.
 */
template <typename Page>
std::string btree_page_label(const Page& page) {
    std::ostringstream os;
    os << "[ ";
    for (Index i{}; i < page.keyCount(); ++i) {
        if (i > 0) os << " │ ";
        os << page.key(i).getValue();
    }
    os << " ]";
    return os.str();
}

/** @brief Formatter por defecto: raíz magenta negrita subrayada, interna cian, hoja amarillo, resaltada con fondo rojo. */
struct DefaultBTreeFormatter {
    /**
     * @brief Etiqueta coloreada según el rol.
     * @tparam Page Tipo de página.
     * @param page Página a etiquetar.
     * @param role Rol semántico.
     * @return Etiqueta con secuencias ANSI.
     */
    template <typename Page>
    std::string operator()(const Page& page, BTreeRole role) const {
        std::string text = btree_page_label(page);
        switch (role) {
            case BTreeRole::Root:      return styled(text).magenta().bold().underline().str();
            case BTreeRole::Internal:  return styled(text).cyan().str();
            case BTreeRole::Leaf:      return styled(text).yellow().str();
            case BTreeRole::Highlight: return styled(text).bg_red().white().bold().str();
        }
        return text;
    }
};

/** @brief Formatter sin códigos ANSI (útil para pruebas, archivos y terminales sin color). */
struct PlainBTreeFormatter {
    /**
     * @brief Etiqueta sin color.
     * @tparam Page Tipo de página.
     * @param page Página a etiquetar.
     * @param role Rol semántico (ignorado).
     * @return Etiqueta plana.
     */
    template <typename Page>
    std::string operator()(const Page& page, [[maybe_unused]] BTreeRole role) const { return btree_page_label(page); }
};

/**
 * @class BTreePageSet
 * @brief Conjunto de punteros no propietarios a páginas para resaltar.
 * @warning Use el conjunto mientras las páginas sigan vigentes y proteja
 * con el mutex del árbol cualquier secuencia concurrente de búsqueda y dibujo.
 * @tparam Page Tipo de página.
 */
template <typename Page>
class BTreePageSet {
    std::vector<const Page*> m_pages;
public:
    BTreePageSet() = default;
    /** @brief Construye el conjunto a partir de una lista de páginas. @param pages Páginas iniciales. */
    BTreePageSet(std::initializer_list<const Page*> pages) : m_pages(pages) {}
    /** @brief Agrega una página (se ignora `nullptr`). @param page Página a agregar. */
    void add(const Page* page) { if (page) m_pages.push_back(page); }
    /** @brief Pertenencia por identidad. @param page Página consultada. @return `true` si `page` está en el conjunto. */
    bool contains(const Page& page) const { return std::find(m_pages.begin(), m_pages.end(), &page) != m_pages.end(); }
    /** @brief Cantidad de páginas del conjunto. @return Tamaño del conjunto. */
    Size size() const { return m_pages.size(); }
};

/**
 * @class BTreeHighlight
 * @brief Formatter combinador: asigna `BTreeRole::Highlight` a las páginas del conjunto y delega en `Inner`.
 * @tparam Page Tipo de página.
 * @tparam Inner Formatter interno con firma `(const Page&, BTreeRole) -> std::string`.
 */
template <typename Page, typename Inner>
class BTreeHighlight {
    BTreePageSet<Page> m_pages;
    Inner              m_inner;
public:
    /**
     * @brief Construye el decorador.
     * @param pages Páginas a resaltar.
     * @param inner Formatter interno.
     */
    BTreeHighlight(BTreePageSet<Page> pages, Inner inner) : m_pages(std::move(pages)), m_inner(std::move(inner)) {}

    /**
     * @brief Formatea `page`, con rol `Highlight` si pertenece al conjunto.
     * @param page Página a etiquetar.
     * @param role Rol calculado por el layout.
     * @return Etiqueta producida por el formatter interno.
     */
    std::string operator()(const Page& page, BTreeRole role) const {
        return m_inner(page, m_pages.contains(page) ? BTreeRole::Highlight : role);
    }
};

/**
 * @brief Crea un formatter que resalta `pages` y formatea el resto con `inner`.
 * @tparam Page Tipo de página.
 * @tparam Inner Tipo del formatter interno.
 * @param pages Páginas a resaltar.
 * @param inner Formatter interno (por defecto `DefaultBTreeFormatter`).
 * @return Formatter `BTreeHighlight` listo para `BTreeLayout`.
 */
template <typename Page, typename Inner = DefaultBTreeFormatter>
BTreeHighlight<Page, Inner> highlight_pages(BTreePageSet<Page> pages, Inner inner = {}) {
    return BTreeHighlight<Page, Inner>(std::move(pages), std::move(inner));
}

/**
 * @brief Páginas visitadas al buscar `value` (desde la raíz hasta la página que la contiene o la hoja donde terminaría).
 * @tparam Tree Tipo de árbol (`BTree` / `BStarTree` / derivado).
 * @param tree Árbol B (bloquea su mutex durante el descenso).
 * @param value Clave buscada.
 * @return Una página por nivel recorrido.
 */
template <typename Tree>
BTreePageSet<typename Tree::Page> btree_search_path(const Tree& tree, const typename Tree::value_type& value) {
    std::lock_guard lock(tree.mutex());
    BTreePageSet<typename Tree::Page> path;
    for (const auto* page = tree.getRoot(); page; ) {
        path.add(page);
        Index pos = page->lowerBound(value);
        if (page->matchesAt(pos, value) || page->isLeaf()) break;
        page = page->child(pos);
    }
    return path;
}

/**
 * @brief Página que contiene `value` (conjunto vacío si no existe).
 * @tparam Tree Tipo de árbol (`BTree` / `BStarTree` / derivado).
 * @param tree Árbol B (bloquea su mutex durante el descenso).
 * @param value Clave buscada.
 * @return Conjunto con 0 o 1 página.
 */
template <typename Tree>
BTreePageSet<typename Tree::Page> btree_page_of(const Tree& tree, const typename Tree::value_type& value) {
    std::lock_guard lock(tree.mutex());
    BTreePageSet<typename Tree::Page> found;
    for (const auto* page = tree.getRoot(); page; ) {
        Index pos = page->lowerBound(value);
        if (page->matchesAt(pos, value)) { found.add(page); break; }
        if (page->isLeaf()) break;
        page = page->child(pos);
    }
    return found;
}

/** @brief Nodo del borrador: una página con su profundidad y los ids de sus hijos. @tparam Page Tipo de página. */
template <typename Page>
struct BTreeDraftNode : DraftNode<const Page*, BTreeRole> {   // `payload == nullptr` en nodos conectores (modo `Bus`)
    TX depth = 0;                 /**< Profundidad (raíz = 0). */
    std::vector<TX> children;     /**< Ids de los nodos hijos, de izquierda a derecha. */
};

/**
 * @class BTreeLayout
 * @ingroup layouts
 * @brief Proyecta las páginas de un árbol B de arriba hacia abajo.
 *
 * Usa el pipeline CRTP de `BaseLayout`. Visita las páginas por niveles,
 * separa las hojas según el ancho visible de sus etiquetas y centra cada
 * padre entre sus hijos extremos. Desplaza subárboles al detectar
 * solapamientos y genera conectores según `BTreeLinks`.
 *
 * @tparam Formatter Formateador `(const Page&, BTreeRole) -> std::string`.
 */
template <typename Formatter = DefaultBTreeFormatter>
class BTreeLayout : public BaseLayout<BTreeLayout<Formatter>, Formatter> {
public:
    using Role = BTreeRole;
    using Base = BaseLayout<BTreeLayout<Formatter>, Formatter>;
    friend class BaseLayout<BTreeLayout<Formatter>, Formatter>;

private:
    TX         m_vgap  = 3;                 /**< Filas entre niveles (`>= 2`: el bus ocupa una fila). */
    TX         m_hgap  = 2;                 /**< Columnas mínimas entre páginas vecinas del mismo nivel. */
    BTreeLinks m_links = BTreeLinks::Bus;   /**< Forma de dibujar las conexiones. */

    /** @brief Valida las separaciones. @throws std::invalid_argument Si `vgap < 2` o `hgap < 1`. */
    void validate() const {
        if (m_vgap < 2 || m_hgap < 1) throw std::invalid_argument("BTreeLayout: vgap >= 2 y hgap >= 1");
    }

    /**
     * @brief Ancho visible de una etiqueta (ignora ANSI) en el tipo de coordenadas del lienzo (`TX`).
     *
     * Punto único de conversión `Size`→`TX` (entero a entero, sin cast): las
     * etiquetas de una página nunca se acercan al límite de `TX`.
     * @param label Etiqueta (puede contener ANSI).
     * @return Columnas visibles.
     */
    static TX width_of(const std::string& label) {
        TX width = visible_length(label);
        return width;
    }

public:
    /**
     * @brief Layout con el formatter por defecto.
     * @param vgap Filas entre niveles.
     * @param hgap Columnas mínimas entre páginas del mismo nivel.
     */
    explicit BTreeLayout(TX vgap = 3, TX hgap = 2) : Base(false), m_vgap(vgap), m_hgap(hgap) { validate(); }

    /**
     * @brief Layout con un formatter propio (lambda `(page, role)` o `highlight_pages(...)`).
     * @param f Formatter.
     * @param vgap Filas entre niveles.
     * @param hgap Columnas mínimas entre páginas del mismo nivel.
     */
    explicit BTreeLayout(Formatter f, TX vgap = 3, TX hgap = 2) : Base(std::move(f), false), m_vgap(vgap), m_hgap(hgap) { validate(); }

    /**
     * @brief Copia del layout con otra forma de conexión (`BTreeLayout{}.with_links(BTreeLinks::Edges)`).
     * @param links Forma de dibujar las conexiones.
     * @return Layout con la misma configuración y `links`.
     */
    BTreeLayout with_links(BTreeLinks links) const {
        BTreeLayout copy = *this;
        copy.m_links = links;
        return copy;
    }

    /** @brief Forma de conexión configurada. @return `Bus` o `Edges`. */
    BTreeLinks links() const { return m_links; }

private:
    /**
     * @brief Etapa 1 del pipeline: un nodo por página (por niveles) y, en modo `Edges`, una arista por hijo.
     * @tparam Tree Tipo de árbol.
     * @param tree Árbol a proyectar.
     * @return Borrador con nodos, aristas y profundidad máxima.
     */
    template <typename Tree>
    auto extract_topology_impl(const Tree& tree) const;

    /**
     * @brief Etapa 3 del pipeline: coordenadas (hojas con cursor, internas centradas en post-orden).
     * @tparam Draft Tipo del borrador.
     * @param draft Borrador con etiquetas ya formateadas.
     */
    template <typename Draft>
    void compute_coordinates_impl(Draft& draft) const;

    /**
     * @brief Modo `Bus`: agrega, por cada página interna, el nodo del bus y los de las barras verticales.
     * @tparam Draft Tipo del borrador.
     * @param draft Borrador con coordenadas de página ya calculadas.
     */
    template <typename Draft>
    void add_bus_connectors(Draft& draft) const;
};

// ============================================================================
// Implementación fuera de clase: BTreeLayout
// ============================================================================

template <typename Formatter>
template <typename Tree>
auto BTreeLayout<Formatter>::extract_topology_impl(const Tree& tree) const {
    using Page  = typename Tree::Page;
    using Node  = BTreeDraftNode<Page>;
    LayoutDraft<Node> draft;
    const Page* root = tree.getRoot();
    if (!root) return draft;

    auto roleOf = [](const Page& page) {
        if (page.isRoot()) return BTreeRole::Root;
        return page.isLeaf() ? BTreeRole::Leaf : BTreeRole::Internal;
    };

    Node first;
    first.id      = 0;
    first.payload = root;
    first.role    = roleOf(*root);
    draft.nodes.push_back(first);

    TX nextId = 1;
    for (Index cursor{}; cursor < draft.nodes.size(); ++cursor) {
        const Page* page  = draft.nodes[cursor].payload;
        TX          depth = draft.nodes[cursor].depth;
        TX          id    = draft.nodes[cursor].id;
        for (Index i{}; i < page->childCount(); ++i) {
            Node child;
            child.id      = nextId++;
            child.payload = page->child(i);
            child.role    = roleOf(*child.payload);
            child.depth   = depth + 1;
            draft.max_depth = std::max(draft.max_depth, child.depth);
            if (m_links == BTreeLinks::Edges) draft.edges.push_back(VisualEdge{id, child.id});
            draft.nodes.push_back(std::move(child));
            draft.nodes[cursor].children.push_back(nextId - 1);   // después del push (posible realocación)
        }
    }
    return draft;
}

template <typename Formatter>
template <typename Draft>
void BTreeLayout<Formatter>::compute_coordinates_impl(Draft& draft) const {
    auto& nodes = draft.nodes;
    if (nodes.empty()) return;
    for (auto& n : nodes) n.y = n.depth * m_vgap;

    std::vector<TX> nextFree(draft.max_depth + 1, 0);   // primera columna libre por nivel

    auto shift = [&](auto& self, TX id, TX delta) -> void {
        auto* n = find_node(nodes, id);
        n->x += delta;
        for (TX c : n->children) self(self, c, delta);
    };

    auto place = [&](auto& self, TX id) -> void {
        auto* n   = find_node(nodes, id);
        TX    len = width_of(n->label);
        if (n->children.empty()) {
            n->x = nextFree[n->depth];
        } else {
            for (TX c : n->children) self(self, c);
            const auto* firstChild = find_node(nodes, n->children.front());
            const auto* lastChild  = find_node(nodes, n->children.back());
            TX firstCenter = firstChild->x + width_of(firstChild->label) / 2;
            TX lastCenter  = lastChild->x  + width_of(lastChild->label)  / 2;
            n->x = (firstCenter + lastCenter) / 2 - len / 2;
        }
        TX deficit = nextFree[n->depth] - n->x;
        if (deficit > 0) {
            shift(shift, id, deficit);
            for (TX d = n->depth + 1; d <= draft.max_depth; ++d) nextFree[d] += deficit;
        }
        nextFree[n->depth] = n->x + len + m_hgap;
    };
    place(place, 0);
    if (m_links == BTreeLinks::Bus) add_bus_connectors(draft);
}

template <typename Formatter>
template <typename Draft>
void BTreeLayout<Formatter>::add_bus_connectors(Draft& draft) const {
    using Node = typename decltype(draft.nodes)::value_type;
    auto centerOf = [](const Node& n) { return n.x + width_of(n.label) / 2; };
    TX nextId = draft.nodes.back().id + 1;
    const Index pages = draft.nodes.size();
    for (Index p{}; p < pages; ++p) {
        if (draft.nodes[p].children.empty()) continue;
        std::vector<TX> centers;
        for (TX c : draft.nodes[p].children) centers.push_back(centerOf(*find_node(draft.nodes, c)));
        TX parentCenter = centerOf(draft.nodes[p]);
        TX first = std::min(centers.front(), parentCenter);
        TX last  = std::max(centers.back(),  parentCenter);

        std::string bus, bars;
        for (TX col = first; col <= last; ++col) {
            bool isChild  = std::find(centers.begin(), centers.end(), col) != centers.end();
            bool isParent = col == parentCenter;
            if      (isChild && isParent)        bus += "┼";
            else if (isParent)                   bus += "┴";
            else if (col == centers.front())     bus += "╭";
            else if (col == centers.back())      bus += "╮";
            else if (isChild)                    bus += "┬";
            else                                 bus += "─";
            bars += isChild ? "│" : " ";
        }
        TX y = draft.nodes[p].y;
        auto connector = [&](std::string label, TX row) {
            Node n;
            n.id    = nextId++;
            n.label = std::move(label);
            n.x     = first;
            n.y     = row;
            n.depth = draft.nodes[p].depth;
            draft.nodes.push_back(std::move(n));
        };
        connector(bus, y + 1);
        for (TX row = y + 2; row < y + m_vgap; ++row) connector(bars, row);
    }
}

// Deduction guides para BTreeLayout
template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
BTreeLayout(F) -> BTreeLayout<std::decay_t<F>>;

template <typename F>
requires (!std::is_integral_v<std::decay_t<F>> && !std::is_same_v<std::decay_t<F>, bool>)
BTreeLayout(F, TX, TX) -> BTreeLayout<std::decay_t<F>>;

#endif // __LAYOUTS_BTREE_LAYOUT_H__
