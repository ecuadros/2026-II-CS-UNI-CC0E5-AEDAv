#ifndef CONTAINERS_VISUALIZATION_H
#define CONTAINERS_VISUALIZATION_H

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <sys/ioctl.h>
#include <unistd.h>

/** Dibujo de contenedores en la terminal: árboles, listas y arreglos, con color opcional. */
namespace visual {

/** Vista de árbol: da la raíz, los hijos de un nodo, si un nodo es válido y su etiqueta. */
template <class V>
concept TreeView = requires(const V& v, typename V::Handle h, std::ostream& os) {
    { v.root() } -> std::same_as<typename V::Handle>;
    { v.child(h, true) } -> std::same_as<typename V::Handle>;
    { v.valid(h) } -> std::convertible_to<bool>;
    v.label(os, h);
};

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
inline constexpr std::string_view edge = "2;37";
inline constexpr std::string_view value = "1;36";
inline constexpr std::string_view index = "2;33";
inline constexpr std::string_view head = "1;32";
inline constexpr std::string_view end = "1;31";
inline constexpr std::string_view loop = "1;35";
inline constexpr std::string_view levels[] = {"1;93", "1;96", "1;92", "1;95", "1;94", "1;91"};
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
    static bool enabled(const std::ostream& os) {
        if (color_mode != ColorMode::Auto) return color_mode == ColorMode::Always;
        if (std::getenv("NO_COLOR")) return false;
        return (&os == &std::cout && isatty(fileno(stdout))) || (&os == &std::cerr && isatty(fileno(stderr)));
    }

    bool m_on;
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

/** Dibuja recursivamente un subárbol tumbado: la rama derecha arriba y la izquierda abajo. @p dir indica de qué lado cuelga el nodo: 1 derecha, 0 izquierda, -1 sin rama. */
template <TreeView V>
void sideways(std::ostream& os, const Painter& paint, const V& v, typename V::Handle h, const std::string& prefix,
              int dir, std::size_t depth) {
    if (!v.valid(h)) return;
    sideways(os, paint, v, v.child(h, true), prefix + (dir < 0 ? "" : dir == 0 ? "│     " : "      "), 1, depth + 1);
    os << paint(style::edge, prefix + (dir < 0 ? "" : dir ? "┌──── " : "└──── "));
    os << paint.level(depth, label(v, h)) << '\n';
    sideways(os, paint, v, v.child(h, false), prefix + (dir < 0 ? "" : dir == 1 ? "│     " : "      "), 0, depth + 1);
}

/** Lienzo de celdas de texto sobre el que se compone el dibujo antes de imprimirlo. */
class Canvas {
public:
    /** Celda del lienzo: su texto y su color. */
    struct Cell {
        std::string text = " ";
        std::string_view style;
        /** Indica si la celda está vacía. */
        bool blank() const { return text == " "; }
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
    void print_row(std::ostream& os, const Painter& paint, std::size_t y, bool skip_leading = false) const {
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
    /** Columna central de la etiqueta. */
    std::ptrdiff_t center() const { return x + static_cast<std::ptrdiff_t>(text.size()) / 2; }
};

/** Calcula la posición de cada nodo del subárbol, separando los subárboles hermanos lo justo para que no se solapen. */
template <TreeView V> Box layout(const V& v, typename V::Handle h) {
    Box box{label(v, h)};
    bool only_right = false;
    for (bool right : {false, true})
        if (auto c = v.child(h, right); v.valid(c)) {
            box.kids.push_back(layout(v, c));
            only_right = right && box.kids.size() == 1;
        }

    auto width = static_cast<std::ptrdiff_t>(box.text.size());
    std::ptrdiff_t center = width / 2;
    if (box.kids.size() == 2) {
        auto &l = box.kids[0], &r = box.kids[1];
        auto shift = std::numeric_limits<std::ptrdiff_t>::min();
        for (std::size_t i = 0; i < std::min(l.contour.size(), r.contour.size()); ++i)
            shift = std::max(shift, l.contour[i].second + 2 - r.contour[i].first);
        r.dx = shift;
        center = (l.center() + r.dx + r.center()) / 2;
    } else if (box.kids.size() == 1) {
        center = box.kids[0].center() + (only_right ? -2 : 2);
    }

    box.x = center - width / 2;
    box.contour.push_back({box.x, box.x + width - 1});
    for (const auto& k : box.kids)
        for (std::size_t i = 0; i < k.contour.size(); ++i) {
            std::pair range{k.contour[i].first + k.dx, k.contour[i].second + k.dx};
            if (box.contour.size() <= i + 1)
                box.contour.push_back(range);
            else
                box.contour[i + 1] = {std::min(box.contour[i + 1].first, range.first),
                                      std::max(box.contour[i + 1].second, range.second)};
        }
    return box;
}

/** Pinta en el lienzo un subárbol ya medido y las líneas que lo unen con sus hijos. */
inline void paint_box(Canvas& canvas, const Box& b, std::ptrdiff_t ox, std::size_t depth) {
    ox += b.dx;
    auto y = static_cast<std::ptrdiff_t>(2 * depth);
    canvas.label(ox + b.x, y, b.text, Painter::level_code(depth));
    if (b.kids.empty()) return;

    auto c = ox + b.center(), lo = c, hi = c;
    for (const auto& k : b.kids) {
        paint_box(canvas, k, ox, depth + 1);
        lo = std::min(lo, ox + k.dx + k.center());
        hi = std::max(hi, ox + k.dx + k.center());
    }
    constexpr auto edge = style::edge;
    canvas.hline(lo, hi, y + 1, "─", edge);
    if (lo < c) canvas.glyph(lo, y + 1, "┌", edge);
    if (hi > c) canvas.glyph(hi, y + 1, "┐", edge);
    canvas.glyph(c, y + 1, lo < c && hi > c ? "┴" : lo < c ? "┘" : "└", edge);
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
    int fd = &os == &std::cout ? STDOUT_FILENO : &os == &std::cerr ? STDERR_FILENO : -1;
    winsize ws{};
    if (fd >= 0 && isatty(fd) && ioctl(fd, TIOCGWINSZ, &ws) == 0) return ws.ws_col;
    return 0;
}

/** Indica si el lienzo cabe en el ancho de la terminal. */
inline bool fits(const std::ostream& os, const Canvas& canvas) {
    auto cols = terminal_width(os);
    return cols == 0 || canvas.width() <= cols;
}

/** Dibuja una lista enlazada como cadena `head -> ... -> null`. */
template <LinearView C> void print_chain(std::ostream& os, const C& c) {
    Painter paint(os);
    std::string arrow = paint(style::edge, DoubleLinkedView<C> ? " <-> " : " -> ");
    os << paint(style::head, "head");
    for (auto it = c.begin(); it != c.end(); ++it)
        os << arrow << paint(style::value, to_text(*it));
    if constexpr (CircularView<C>)
        os << arrow << paint(style::loop, "(vuelve a head)");
    else
        os << arrow << paint(style::end, "null");
    os << '\n';
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
        bool last = i + 1 == cells.size();
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

/** Cantidad de nodos del subárbol. */
template <TreeView V> std::size_t count(const V& v, typename V::Handle h) {
    return v.valid(h) ? 1 + count(v, v.child(h, true)) + count(v, v.child(h, false)) : 0;
}

/** Dibuja el árbol tumbado, con la raíz a la izquierda. */
template <TreeView V> void print_sideways(std::ostream& os, const V& v) {
    Painter paint(os);
    auto root = v.root();
    auto up = count(v, v.child(root, true)), down = count(v, v.child(root, false));
    sideways(os, paint, v, v.child(root, true), "", 1, 1);
    for (auto i = up; i < down; ++i)
        os << paint(style::edge, up ? "│" : "") << '\n';
    os << paint.level(0, label(v, root)) << '\n';
    for (auto i = down; i < up; ++i)
        os << paint(style::edge, down ? "│" : "") << '\n';
    sideways(os, paint, v, v.child(root, false), "", 0, 1);
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

} // namespace visual

#endif // CONTAINERS_VISUALIZATION_H
