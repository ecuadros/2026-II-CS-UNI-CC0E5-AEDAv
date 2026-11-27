#ifndef CONTAINERS_VISUALIZATION_H
#define CONTAINERS_VISUALIZATION_H

#include "../../types.h"
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <limits>
#include <ostream>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <sys/ioctl.h>
#include <unistd.h>

/** Dibujo de contenedores en la terminal: árboles, listas y arreglos, con color opcional. */
namespace visual {

/** Vista de árbol: da la raíz, los hijos de un nodo, si un nodo es válido y su etiqueta. */
template <class V>
concept TreeViewBase = requires(const V& v, typename V::Handle h, std::ostream& os) {
    { v.root() } -> std::same_as<typename V::Handle>;
    { v.valid(h) } -> std::convertible_to<Bool>;
    v.label(os, h);
};

/** Vista de árbol binario: `child(h, side)` da el hijo izquierdo (`false`) o derecho (`true`). */
template <class V>
concept BinaryTreeView = TreeViewBase<V> && requires(const V& v, typename V::Handle h) {
    { v.child(h, true) } -> std::same_as<typename V::Handle>;
};

/** Vista de árbol de varios hijos (como un árbol B): `children(h)` da los hijos de izquierda a derecha. */
template <class V>
concept MultiwayTreeView = TreeViewBase<V> && requires(const V& v, typename V::Handle h) {
    { v.children(h) } -> std::ranges::input_range;
    requires std::convertible_to<std::ranges::range_value_t<decltype(v.children(h))>, typename V::Handle>;
};

/** Vista de árbol que se puede dibujar: binaria o de varios hijos. */
template <class V>
concept TreeView = BinaryTreeView<V> || MultiwayTreeView<V>;

/** Contenedor que ofrece una vista de árbol con `tree_view()`. */
template <class C>
concept HasTreeView = requires(const C& c) {
    { c.tree_view() } -> TreeView;
};

/** Contenedor recorrible cuyos elementos se pueden imprimir. */
template <class C>
concept LinearView = requires(const C& c, std::ostream& os) {
    c.begin();
    c.end();
    os << *c.begin();
};

/** Contenedor que se dibuja en un stream con `print(os)`. */
template <class C>
concept PrintableView = requires(const C& c, std::ostream& os) {
    c.print(os);
};

/** Vista de tabla: da las dimensiones, el encabezado de cada columna y el texto de cada celda. */
template <class V>
concept TableView = requires(const V& v, Int row, Int col) {
    { v.rows() } -> std::convertible_to<Int>;
    { v.cols() } -> std::convertible_to<Int>;
    { v.col_header(col) } -> std::convertible_to<std::string>;
    { v.cell_text(row, col) } -> std::convertible_to<std::string>;
};

/**
 * Contenedor que ofrece una vista de tabla con `table_view()`.
 * `table_view()` debe devolver una vista autocontenida y segura frente a accesos concurrentes.
 */
template <class C>
concept HasTableView = requires(const C& c) {
    { c.table_view() } -> TableView;
};

/** Secuencia cuyos nodos tienen `m_pNext` (lista enlazada). */
template <class C>
concept LinkedView = LinearView<C> && requires(const typename C::Node& node) { node.m_pNext; };

/** Lista cuyos nodos tienen además `m_pPrev`. */
template <class C>
concept DoubleLinkedView = LinkedView<C> && requires(const typename C::Node& node) { node.m_pPrev; };

/** Lista enlazada marcada como circular. */
template <class C>
concept CircularView = LinkedView<C> && C::is_circular;

/** Cuándo usar colores ANSI: automático según la terminal, siempre o nunca. */
enum class ColorMode { Auto, Always, Never };

/** Disposición del dibujo de un árbol: vertical, tumbado o vertical redibujado en la posición del cursor. */
enum class Layout { Vertical, Sideways, Cursor };

/** Modo de color global. */
inline ColorMode color_mode = ColorMode::Auto;

/** Códigos de color ANSI de cada elemento del dibujo. */
namespace style {
inline constexpr std::string_view edge = "38;5;103";
inline constexpr std::string_view value = "1;36";
inline constexpr std::string_view index = "2;33";
inline constexpr std::string_view head = "1;32";
inline constexpr std::string_view end = "1;31";
inline constexpr std::string_view loop = "1;35";
inline constexpr std::string_view levels[] = {"1;38;5;220", "1;38;5;87", "1;38;5;120", "1;38;5;177", "1;38;5;111", "1;38;5;210"};
} // namespace style

/** Envuelve texto en secuencias de color ANSI, solo si el color está activo para el stream. */
class Painter {
public:
    /** Decide si se usa color al escribir en @p os. */
    explicit Painter(const std::ostream& os) : m_on(enabled(os)) {}

    /** Devuelve @p text con el color @p code, o sin cambios si el color está desactivado. */
    std::string operator()(std::string_view code, std::string_view text) const {
        if (!m_on || code.empty() || text.empty()) return std::string(text);
        return "\033[" + std::string(code) + 'm' + std::string(text) + "\033[0m";
    }

    /** Código de color del nivel @p depth del árbol; los colores se repiten cíclicamente. */
    static std::string_view level_code(std::size_t depth) { return style::levels[depth % std::size(style::levels)]; }

    /** Colorea @p text con el color del nivel @p depth. */
    std::string level(std::size_t depth, std::string_view text) const { return (*this)(level_code(depth), text); }

private:
    /** Determina si hay color: respeta `color_mode` y `NO_COLOR`, y en modo automático exige que @p os sea una terminal. */
    static Bool enabled(const std::ostream& os) {
        if (color_mode != ColorMode::Auto) return color_mode == ColorMode::Always;
        if (std::getenv("NO_COLOR")) return false;
        return (&os == &std::cout && isatty(fileno(stdout))) || (&os == &std::cerr && isatty(fileno(stderr)));
    }

    Bool m_on;
};

/** Repite la cadena @p s @p n veces. */
inline std::string repeat(const std::string& s, std::ptrdiff_t n) {
    std::string out;
    for (std::ptrdiff_t i = 0; i < n; ++i)
        out += s;
    return out;
}

/** Convierte un valor a texto con `operator<<`. */
template <class T> std::string to_text(const T& value) {
    std::ostringstream ss;
    ss << value;
    return ss.str();
}

/** Detalles de implementación del dibujo. */
namespace detail {

/** Devuelve la etiqueta de un nodo como texto. */
template <TreeView V> std::string label(const V& v, typename V::Handle h) {
    std::ostringstream ss;
    v.label(ss, h);
    return ss.str();
}

/** Hijos de @p h de izquierda a derecha, incluidos los no válidos para conservar su posición. */
template <TreeView V> std::vector<typename V::Handle> children(const V& v, typename V::Handle h) {
    if constexpr (MultiwayTreeView<V>) {
        auto&& kids = v.children(h);
        return {std::ranges::begin(kids), std::ranges::end(kids)};
    } else {
        return {v.child(h, false), v.child(h, true)};
    }
}

/** Indica si el hijo @p i de @p n se dibuja arriba del padre en el dibujo tumbado: la mitad derecha. */
inline Bool upper_half(std::size_t i, std::size_t n) { return 2 * i >= n; }

/** Cantidad de nodos del subárbol. */
template <TreeView V> std::size_t count(const V& v, typename V::Handle h) {
    if (!v.valid(h)) return 0;
    std::size_t n = 1;
    for (auto kid : children(v, h))
        n += count(v, kid);
    return n;
}

/**
 * Dibuja recursivamente un subárbol tumbado: los hijos de la derecha arriba y los de la izquierda abajo.
 * @p above, @p line y @p below son los prefijos de las filas sobre el nodo, de la fila del nodo y de las filas bajo él.
 * Si @p balance, rellena con líneas para que el nodo quede a media altura entre sus dos mitades.
 */
template <TreeView V>
void sideways(std::ostream& os, const Painter& paint, const V& v, typename V::Handle h, const std::string& above,
              const std::string& line, const std::string& below, std::size_t depth, Bool balance = false) {
    std::vector<typename V::Handle> upper, lower;
    auto kids = children(v, h);
    for (std::size_t i = kids.size(); i-- > 0;)
        if (v.valid(kids[i])) (upper_half(i, kids.size()) ? upper : lower).push_back(kids[i]);

    std::size_t up = 0, down = 0;
    for (std::size_t k = 0; k < upper.size(); ++k) {
        Bool top = k == 0;
        sideways(os, paint, v, upper[k], above + (top ? "      " : "│     "), above + (top ? "┌──── " : "├──── "),
                 above + "│     ", depth + 1);
        up += balance ? count(v, upper[k]) : 0;
    }
    for (auto kid : lower)
        down += balance ? count(v, kid) : 0;
    for (auto i = up; i < down; ++i)
        os << paint(style::edge, above + (up ? "│" : "")) << '\n';
    os << paint(style::edge, line) << paint.level(depth, label(v, h)) << '\n';
    for (auto i = down; i < up; ++i)
        os << paint(style::edge, below + (down ? "│" : "")) << '\n';
    for (std::size_t k = 0; k < lower.size(); ++k) {
        Bool bottom = k + 1 == lower.size();
        sideways(os, paint, v, lower[k], below + "│     ", below + (bottom ? "└──── " : "├──── "),
                 below + (bottom ? "      " : "│     "), depth + 1);
    }
}

/** Lienzo de celdas de texto sobre el que se compone el dibujo antes de imprimirlo. */
class Canvas {
public:
    /** Celda del lienzo: su texto y su color. */
    struct Cell {
        std::string text = " ";
        std::string_view style;
        /** Indica si la celda está vacía. */
        Bool blank() const { return text == " "; }
    };

    /** Escribe @p text a partir de la celda (@p x, @p y). */
    void label(std::ptrdiff_t x, std::ptrdiff_t y, std::string_view text, std::string_view style) {
        auto& row = row_at(y, x + static_cast<std::ptrdiff_t>(text.size()));
        row[x] = {std::string(text), style};
        for (std::size_t i = 1; i < text.size(); ++i)
            row[x + i] = {"", style};
    }

    /** Pone un único símbolo en la celda (@p x, @p y). */
    void glyph(std::ptrdiff_t x, std::ptrdiff_t y, std::string_view g, std::string_view style) {
        row_at(y, x + 1)[x] = {std::string(g), style};
    }

    /** Dibuja una línea horizontal entre las columnas @p from y @p to. */
    void hline(std::ptrdiff_t from, std::ptrdiff_t to, std::ptrdiff_t y, std::string_view g, std::string_view style) {
        for (auto x = from; x <= to; ++x)
            glyph(x, y, g, style);
    }

    /** Filas del lienzo, para copiarlo celda a celda a otra superficie. */
    const std::vector<std::vector<Cell>>& rows() const { return m_rows; }

    /** Cantidad de filas. */
    std::size_t height() const { return m_rows.size(); }

    /** Ancho de la fila más larga. */
    std::size_t width() const {
        std::size_t w = 0;
        for (const auto& row : m_rows)
            w = std::max(w, row.size());
        return w;
    }

    /** Imprime el lienzo fila por fila. */
    void print(std::ostream& os, const Painter& paint) const {
        for (std::size_t y = 0; y < height(); ++y)
            print_row(os, paint, y);
    }

    /** Imprime el lienzo sobre la posición guardada del cursor, sin borrar lo que haya fuera del dibujo. */
    void print_at_cursor(std::ostream& os, const Painter& paint) const {
        for (std::size_t y = 0; y < height(); ++y) {
            os << "\033[u";
            if (y > 0) os << "\033[" << y << 'B';
            if (auto x = first_ink(m_rows[y]); x > 0) os << "\033[" << x << 'C';
            print_row(os, paint, y, true);
        }
    }

private:
    std::vector<std::vector<Cell>> m_rows;

    /** Devuelve la fila @p y, ampliando el lienzo para que tenga al menos @p width columnas. */
    std::vector<Cell>& row_at(std::ptrdiff_t y, std::ptrdiff_t width) {
        if (m_rows.size() <= static_cast<std::size_t>(y)) m_rows.resize(y + 1);
        auto& row = m_rows[y];
        if (row.size() < static_cast<std::size_t>(width)) row.resize(width);
        return row;
    }

    /** Índice de la primera celda no vacía de la fila. */
    static std::size_t first_ink(const std::vector<Cell>& row) {
        return std::find_if(row.begin(), row.end(), [](const Cell& c) { return !c.blank(); }) - row.begin();
    }

    /** Imprime una fila sin los espacios finales. */
    void print_row(std::ostream& os, const Painter& paint, std::size_t y, Bool skip_leading = false) const {
        const auto& row = m_rows[y];
        auto last = std::find_if(row.rbegin(), row.rend(), [](const Cell& c) { return !c.blank(); }).base();
        for (auto it = skip_leading ? row.begin() + first_ink(row) : row.begin(); it < last; ++it)
            os << paint(it->style, it->text);
        if (!skip_leading) os << '\n';
    }
};

/** Subárbol ya medido: etiqueta, posición relativa, contorno por nivel e hijos. */
struct Box {
    std::string text;
    std::ptrdiff_t dx = 0, x = 0;
    std::vector<std::pair<std::ptrdiff_t, std::ptrdiff_t>> contour;
    std::vector<Box> kids;
    /** Ancho total de la caja del nodo (`│ ` + texto + ` │`). */
    std::ptrdiff_t box_width() const { return static_cast<std::ptrdiff_t>(text.size()) + 4; }
    /** Columna central de la caja. */
    std::ptrdiff_t center() const { return x + box_width() / 2; }
};

/** Calcula la posición de cada nodo del subárbol, separando los subárboles hermanos lo justo para que no se solapen. */
template <TreeView V> Box layout(const V& v, typename V::Handle h) {
    Box box{label(v, h), 0, 0, {}, {}};
    auto kids = children(v, h);
    std::size_t only = 0;
    for (std::size_t i = 0; i < kids.size(); ++i)
        if (v.valid(kids[i])) {
            box.kids.push_back(layout(v, kids[i]));
            only = i;
        }

    std::vector<std::pair<std::ptrdiff_t, std::ptrdiff_t>> below;
    for (auto& k : box.kids) {
        if (!below.empty()) {
            auto shift = std::numeric_limits<std::ptrdiff_t>::min();
            for (std::size_t i = 0; i < std::min(below.size(), k.contour.size()); ++i)
                shift = std::max(shift, below[i].second + 2 - k.contour[i].first);
            k.dx = shift;
        }
        for (std::size_t i = 0; i < k.contour.size(); ++i) {
            std::pair range{k.contour[i].first + k.dx, k.contour[i].second + k.dx};
            if (below.size() <= i)
                below.push_back(range);
            else
                below[i] = {std::min(below[i].first, range.first), std::max(below[i].second, range.second)};
        }
    }

    auto width = box.box_width();
    std::ptrdiff_t center = width / 2;
    if (box.kids.size() > 1) {
        const auto &l = box.kids.front(), &r = box.kids.back();
        center = (l.center() + l.dx + r.center() + r.dx) / 2;
    } else if (box.kids.size() == 1) {
        auto side = 2 * static_cast<std::ptrdiff_t>(only) - static_cast<std::ptrdiff_t>(kids.size() - 1);
        center = box.kids[0].center() + (side < 0 ? 2 : side > 0 ? -2 : 0);
    }

    box.x = center - width / 2;
    box.contour.push_back({box.x, box.x + width - 1});
    box.contour.insert(box.contour.end(), below.begin(), below.end());
    return box;
}

/** Pinta en el lienzo un subárbol ya medido como cajas y las líneas que lo unen con sus hijos. */
inline void paint_box(Canvas& canvas, const Box& b, std::ptrdiff_t ox, std::size_t depth) {
    ox += b.dx;
    auto y = static_cast<std::ptrdiff_t>(4 * depth);
    auto x0 = ox + b.x;
    auto w = b.box_width();
    auto node_style = Painter::level_code(depth);
    canvas.glyph(x0, y, "┌", node_style);
    canvas.hline(x0 + 1, x0 + w - 2, y, "─", node_style);
    canvas.glyph(x0 + w - 1, y, "┐", node_style);
    canvas.glyph(x0, y + 1, "│", node_style);
    canvas.label(x0 + 2, y + 1, b.text, node_style);
    canvas.glyph(x0 + w - 1, y + 1, "│", node_style);
    canvas.glyph(x0, y + 2, "└", node_style);
    canvas.hline(x0 + 1, x0 + w - 2, y + 2, "─", node_style);
    canvas.glyph(x0 + w - 1, y + 2, "┘", node_style);
    if (b.kids.empty()) return;

    auto c = ox + b.center(), lo = c, hi = c;
    auto child_style = Painter::level_code(depth + 1);
    std::vector<std::ptrdiff_t> tops;
    for (const auto& k : b.kids) {
        paint_box(canvas, k, ox, depth + 1);
        tops.push_back(ox + k.dx + k.center());
        canvas.glyph(tops.back(), y + 4, "┴", child_style);
        lo = std::min(lo, tops.back());
        hi = std::max(hi, tops.back());
    }
    constexpr auto edge = style::edge;
    canvas.glyph(c, y + 2, "┬", node_style);
    canvas.hline(lo, hi, y + 3, "─", edge);
    Bool down = false;
    for (auto x : tops) {
        if (x == c) down = true;
        else canvas.glyph(x, y + 3, x == lo ? "┌" : x == hi ? "┐" : "┬", edge);
    }
    const Char* joint = lo < c && hi > c ? (down ? "┼" : "┴")
                        : lo < c         ? (down ? "┤" : "┘")
                        : hi > c         ? (down ? "├" : "└")
                                         : "│";
    canvas.glyph(c, y + 3, joint, edge);
}

/** Compone el dibujo vertical completo de un árbol. */
template <TreeView V> Canvas draw(const V& v) {
    Canvas canvas;
    auto box = layout(v, v.root());
    auto left = std::ranges::min(box.contour, {}, [](const auto& r) { return r.first; }).first;
    paint_box(canvas, box, -left, 0);
    return canvas;
}

/** Ancho en columnas de la terminal asociada a @p os; 0 si no es una terminal. */
inline std::size_t terminal_width(const std::ostream& os) {
    Int fd = &os == &std::cout ? STDOUT_FILENO : &os == &std::cerr ? STDERR_FILENO : -1;
    winsize ws{};
    if (fd >= 0 && isatty(fd) && ioctl(fd, TIOCGWINSZ, &ws) == 0) return ws.ws_col;
    return 0;
}

/** Indica si el lienzo cabe en el ancho de la terminal. */
inline Bool fits(const std::ostream& os, const Canvas& canvas) {
    auto cols = terminal_width(os);
    return cols == 0 || canvas.width() <= cols;
}

/** Dibuja una lista enlazada como nodos en caja unidos por flechas. */
template <LinearView C> void print_chain(std::ostream& os, const C& c) {
    Painter paint(os);
    std::string_view raw_arrow = DoubleLinkedView<C> ? " <-> " : " -> ";
    std::string arrow = paint(style::edge, raw_arrow);
    std::vector<std::string> items;
    for (auto it = c.begin(); it != c.end(); ++it)
        items.push_back(to_text(*it));
    if (items.empty()) {
        os << paint(style::head, "head") << arrow;
        if constexpr (CircularView<C>)
            os << paint(style::loop, "(vuelve a head)");
        else
            os << paint(style::end, "null");
        os << '\n';
        return;
    }

    std::string pad_head(4 + raw_arrow.size(), ' ');
    std::string pad_arrow(raw_arrow.size(), ' ');
    std::string top = pad_head, mid = paint(style::head, "head") + arrow, bot = pad_head;
    for (std::size_t i = 0; i < items.size(); ++i) {
        auto inner = static_cast<std::ptrdiff_t>(items[i].size()) + 2;
        std::string h = repeat("─", inner);
        top += paint(style::edge, "┌" + h + "┐");
        mid += paint(style::edge, "│") + " " + paint(style::value, items[i]) + " " + paint(style::edge, "│");
        bot += paint(style::edge, "└" + h + "┘");
        if (i + 1 < items.size()) {
            top += pad_arrow;
            mid += arrow;
            bot += pad_arrow;
        }
    }
    if constexpr (CircularView<C>)
        mid += arrow + paint(style::loop, "(vuelve a head)");
    else
        mid += arrow + paint(style::end, "null");
    os << top << '\n' << mid << '\n' << bot << '\n';
}

/** Dibuja una secuencia como celdas con su índice debajo. */
template <LinearView C> void print_cells(std::ostream& os, const C& c) {
    Painter paint(os);
    std::vector<std::string> cells;
    for (auto it = c.begin(); it != c.end(); ++it)
        cells.push_back(to_text(*it));
    if (cells.empty()) {
        os << paint(style::edge, "[]") << '\n';
        return;
    }

    std::string bar = paint(style::edge, "│");
    std::string top = "┌", middle = bar, bottom = "└", index = " ";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        auto width = static_cast<std::ptrdiff_t>(cells[i].size()) + 2;
        Bool last = i + 1 == cells.size();
        top += repeat("─", width) + (last ? "┐" : "┬");
        middle += " " + paint(style::value, cells[i]) + " " + bar;
        bottom += repeat("─", width) + (last ? "┘" : "┴");

        std::string number = std::to_string(i);
        auto pad = width - static_cast<std::ptrdiff_t>(number.size());
        index += std::string(pad / 2, ' ') + number + std::string(pad - pad / 2 + 1, ' ');
    }
    os << paint(style::edge, top) << '\n'
       << middle << '\n'
       << paint(style::edge, bottom) << '\n'
       << paint(style::index, index) << '\n';
}

/** Dibuja el árbol tumbado, con la raíz a la izquierda. */
template <TreeView V> void print_sideways(std::ostream& os, const V& v) {
    sideways(os, Painter(os), v, v.root(), "", "", "", 0, true);
}

/** Dibuja el árbol en vertical; si no cabe en la terminal, lo dibuja tumbado. */
template <TreeView V> void print_vertical(std::ostream& os, const V& v) {
    auto canvas = draw(v);
    if (!fits(os, canvas)) return print_sideways(os, v);
    canvas.print(os, Painter(os));
}

/** Dibuja el árbol en vertical reservando las líneas y pintando desde el cursor; si no cabe, lo dibuja tumbado. */
template <TreeView V> void print_cursor(std::ostream& os, const V& v) {
    auto canvas = draw(v);
    if (!fits(os, canvas)) return print_sideways(os, v);
    auto rows = canvas.height();
    os << std::string(rows, '\n') << "\033[" << rows << 'A' << "\033[s";
    canvas.print_at_cursor(os, Painter(os));
    os << "\033[u\033[" << rows << 'B' << '\r' << std::flush;
}

} // namespace detail

/** Dibuja un árbol con la disposición indicada; no imprime nada si está vacío. */
template <TreeView V> void print_tree(std::ostream& os, const V& v, Layout layout = Layout::Vertical) {
    if (!v.valid(v.root())) return;
    switch (layout) {
    case Layout::Vertical: detail::print_vertical(os, v); break;
    case Layout::Sideways: detail::print_sideways(os, v); break;
    case Layout::Cursor: detail::print_cursor(os, v); break;
    }
}

/** Dibuja una secuencia: como cadena si es una lista enlazada, como celdas en otro caso. */
template <LinearView C> void print_linear(std::ostream& os, const C& c) {
    if constexpr (LinkedView<C>)
        detail::print_chain(os, c);
    else
        detail::print_cells(os, c);
}

/** Límite máximo de celdas imprimibles en `print_table`. */
inline constexpr std::size_t max_table_cells = 100'000;

/**
 * Comprueba que las dimensiones @p cols x @p rows no superen `max_table_cells`.
 * @throws std::length_error si las dimensiones superan el límite imprimible.
 */
inline void check_table_bounds(Int cols, Int rows) {
    if (cols < 0 || rows < 0) return;
    if (static_cast<std::size_t>(cols) > max_table_cells ||
        static_cast<std::size_t>(rows) > max_table_cells ||
        (cols > 0 && rows > 0 &&
         static_cast<std::size_t>(cols) > max_table_cells / static_cast<std::size_t>(rows))) {
        throw std::length_error("Tabla demasiado grande para imprimir");
    }
}

/**
 * Dibuja una tabla con encabezados de columna y filas numeradas.
 * @throws std::length_error si las dimensiones superan `max_table_cells` (`100'000` celdas).
 */
template <TableView V> void print_table(std::ostream& os, const V& v) {
    constexpr Int row_num_width = 4;
    constexpr std::size_t min_col_width = 10;
    Int cols = v.cols(), rows = v.rows();
    if (cols < 0 || rows < 0) return;
    check_table_bounds(cols, rows);
    std::size_t width = min_col_width;
    for (Int col = 1; col <= cols; ++col) width = std::max(width, v.col_header(col).size());
    for (Int row = 1; row <= rows; ++row)
        for (Int col = 1; col <= cols; ++col) width = std::max(width, v.cell_text(row, col).size());

    Int col_w = static_cast<Int>(width);
    auto put_col = [&](const std::string& s) { os << std::setw(col_w) << s << " |"; };
    os << std::setw(row_num_width + 1) << "" << '|';
    for (Int col = 1; col <= cols; ++col) put_col(v.col_header(col));
    os << '\n' << std::string(row_num_width + 1, '-') << '+';
    std::string sep(width + 1, '-');
    for (Int col = 1; col <= cols; ++col) os << sep << '+';
    os << '\n';
    for (Int row = 1; row <= rows; ++row) {
        os << std::setw(row_num_width) << row << " |";
        for (Int col = 1; col <= cols; ++col) put_col(v.cell_text(row, col));
        os << '\n';
    }
}

} // namespace visual

#endif // CONTAINERS_VISUALIZATION_H
